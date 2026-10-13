import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { CloseCode, defaultHubOptions, Hub, type Connection, type Log } from '../src/hub.js';
import { turnCredentials } from '../src/ice.js';
import { hashToken } from '../src/registry.js';
import type { Access, AccountDevice, ServerMessage, Transport } from '../src/protocol.js';
import { MemoryAccounts, MemoryBroker, MemoryCodes, MemoryLimiter, MemoryMailer, MemoryRegistry, MemoryStats } from './memory.js';

const silent: Log = { info() {}, warn() {}, error() {} };

// 一条假的 WebSocket：记下发给它的消息和关闭
class FakePeer {
    readonly sent: ServerMessage[] = [];
    closedWith: number | null = null;

    constructor(readonly host = 'localhost', readonly address = '203.0.113.1') {}

    send(text: string): void {
        this.sent.push(JSON.parse(text) as ServerMessage);
    }

    close(code: number): void {
        this.closedWith = code;
    }

    // 最近一条这种消息
    last<T extends ServerMessage['type']>(type: T): Extract<ServerMessage, { type: T }> | undefined {
        return this.sent.filter((m) => m.type === type).at(-1) as Extract<ServerMessage, { type: T }> | undefined;
    }
}

function token(n: number): string {
    return String(n).padStart(43, 'T');
}

// 设备密钥的公钥：65 字节，0x04 开头
function publicKey(n: number): string {
    return Buffer.concat([Buffer.from([0x04]), Buffer.alloc(64, n)]).toString('base64');
}

function hello(deviceId: string, n = 1, machine = ''): string {
    return JSON.stringify({
        type: 'hello', deviceId, token: token(n), publicKey: publicKey(n), name: '电脑', platform: 'macOS', version: '0.1',
        machine,
    });
}

function signal(to: string, session: string, text: string, transport: Transport = 'p2p', access: Access = 'password',
                sdk = '0.1.0-beta'): string {
    return JSON.stringify({ type: 'signal', to, session, text, transport, access, sdk });
}

// 等所有排着的异步（数据库、Bus 的 setImmediate）都走完
async function settle(): Promise<void> {
    for (let i = 0; i < 5; ++i)
        await new Promise((resolve) => setImmediate(resolve));
}

// 等到条件成立（算密码要几十毫秒）
async function until(condition: () => boolean): Promise<void> {
    for (let i = 0; i < 200 && !condition(); ++i)
        await new Promise((resolve) => setTimeout(resolve, 10));
    expect(condition()).toBe(true);
}

// 这台设备最近一次收到的账号下的设备
function devicesIn(device: { peer: FakePeer }): AccountDevice[] {
    return device.peer.last('devices')?.devices ?? [];
}

describe('Hub', () => {
    let registry: MemoryRegistry;
    let accounts: MemoryAccounts;
    let broker: MemoryBroker;
    let limiter: MemoryLimiter;
    let codes: MemoryCodes;
    let mailer: MemoryMailer;
    let stats: MemoryStats;
    // 两个实例，共用登记表、账号和 Redis
    let first: Hub;
    let second: Hub;
    const hubs: Hub[] = [];

    // 同一个邮箱再要验证码不用等（测试里同一个邮箱会注册好几次）；要等的另有测试
    const testOptions = { ...defaultHubOptions, limits: { ...defaultHubOptions.limits, codeIntervalS: 0 } };

    function hub(options = testOptions, mail: MemoryMailer | null = mailer): Hub {
        const created = new Hub({ registry, accounts, bus: broker.bus(), limiter, codes, mailer: mail, stats }, silent, options);
        hubs.push(created);
        return created;
    }

    // 限速改小了的实例
    function strictHub(limits: Partial<typeof defaultHubOptions.limits>): Hub {
        return hub({ ...testOptions, limits: { ...testOptions.limits, ...limits } });
    }

    function connect(on: Hub, host?: string, address?: string): { peer: FakePeer; connection: Connection } {
        const peer = new FakePeer(host, address);
        return { peer, connection: on.connect(peer) };
    }

    async function login(on: Hub, deviceId: string, n = 1, host?: string, address?: string, machine = '')
        : Promise<{ peer: FakePeer; connection: Connection }> {
        const device = connect(on, host, address);
        device.connection.receive(hello(deviceId, n, machine));
        await until(() => device.peer.last('account') !== undefined);
        expect(device.peer.sent[0]?.type).toBe('welcome');
        return device;
    }

    function send(device: { connection: Connection }, message: object): void {
        device.connection.receive(JSON.stringify(message));
    }

    // 这台设备从第 from 条消息起收到的第一条 type
    async function next<T extends ServerMessage['type']>(device: { peer: FakePeer }, from: number, ...types: T[])
        : Promise<Extract<ServerMessage, { type: T }>> {
        const found = () => device.peer.sent.slice(from).find((m) => types.includes(m.type as T));
        await until(() => found() !== undefined);
        return found() as Extract<ServerMessage, { type: T }>;
    }

    // 要一个验证码（假的发信器收着），回答是 codeSent 或者 accountFailed
    async function sendCode(device: { connection: Connection; peer: FakePeer }, email: string,
                            purpose: 'register' | 'resetPassword' = 'register') {
        const from = device.peer.sent.length;
        send(device, { type: 'sendCode', email, purpose });
        return next(device, from, 'codeSent', 'accountFailed');
    }

    // 注册：先要验证码，再带着它注册
    async function register(device: { connection: Connection; peer: FakePeer }, email: string, password: string,
                            nickname: string): Promise<void> {
        expect((await sendCode(device, email)).type).toBe('codeSent');
        send(device, { type: 'register', email, password, nickname, code: mailer.codeFor(email.trim().toLowerCase()) });
    }

    beforeEach(() => {
        registry = new MemoryRegistry();
        accounts = new MemoryAccounts();
        broker = new MemoryBroker();
        limiter = new MemoryLimiter();
        codes = new MemoryCodes();
        mailer = new MemoryMailer();
        stats = new MemoryStats();
        first = hub();
        second = hub();
    });

    afterEach(() => {
        for (const created of hubs.splice(0))
            created.close();
        vi.useRealTimers();
    });

    it('registers a new device on hello', async () => {
        const device = await login(first, '482915307');
        expect(registry.devices.get('482915307')?.name).toBe('电脑');
        expect(registry.devices.get('482915307')?.publicKey).toBe(publicKey(1));
        expect(first.onlineCount).toBe(1);
        // 没登录账号，服务器没配中继
        expect(device.peer.last('account')).toEqual({ type: 'account', account: null });
        expect(device.peer.last('welcome')).toEqual({ type: 'welcome', ice: null });
    });

    it('rejects an id that belongs to another device', async () => {
        await login(first, '482915307', 1);
        const other = connect(second);
        other.connection.receive(hello('482915307', 2));
        await settle();
        expect(other.peer.sent).toEqual([{ type: 'rejected', reason: 'idTaken' }]);
        expect(other.peer.closedWith).toBe(CloseCode.IdTaken);
    });

    it('forwards signals between instances and back', async () => {
        const viewer = await login(first, '111111111', 1);
        const sharer = await login(second, '222222222', 2);

        viewer.connection.receive(signal('222222222', 's1', 'offer', 'relay', 'device'));
        await settle();
        expect(sharer.peer.last('signal')).toEqual({
            type: 'signal', from: '111111111', name: '电脑', session: 's1', text: 'offer', transport: 'relay', access: 'device',
            sdk: '0.1.0-beta',
        });

        sharer.connection.receive(signal('111111111', 's1', 'answer', 'relay', 'device'));
        await settle();
        expect(viewer.peer.last('signal')).toEqual({
            type: 'signal', from: '222222222', name: '电脑', session: 's1', text: 'answer', transport: 'relay', access: 'device',
            sdk: '0.1.0-beta',
        });
        // 管理后台的数目：发起了一次连接（回答的那一边不算）
        expect(stats.counts.get('connections')).toBe(1);
    });

    it('sends the new name with later signals after a rename', async () => {
        const viewer = await login(first, '111111111', 1);
        const sharer = await login(second, '222222222', 2);

        send(viewer, { type: 'rename', name: ' 书房的电脑 ' });
        viewer.connection.receive(signal('222222222', 's1', 'offer'));
        await settle();
        expect(registry.devices.get('111111111')?.name).toBe('书房的电脑');
        expect(sharer.peer.last('signal')).toEqual({
            type: 'signal', from: '111111111', name: '书房的电脑', session: 's1', text: 'offer', transport: 'p2p',
            access: 'password', sdk: '0.1.0-beta',
        });
    });

    it('passes a decline back to the device that tried to connect', async () => {
        const viewer = await login(first, '111111111', 1);
        const sharer = await login(second, '222222222', 2);

        send(sharer, { type: 'decline', to: '111111111', session: 's1', reason: 'off' });
        await settle();
        expect(viewer.peer.last('decline')).toEqual({ type: 'decline', from: '222222222', session: 's1', reason: 'off' });
    });

    it('tells the sender when the other device is offline', async () => {
        const viewer = await login(first, '111111111');
        viewer.connection.receive(signal('333333333', 's1', 'offer'));
        await settle();
        expect(viewer.peer.last('undeliverable')).toEqual({ type: 'undeliverable', to: '333333333', session: 's1', reason: 'offline' });
    });

    it('goes offline when the connection closes', async () => {
        const sharer = await login(second, '222222222', 2);
        const viewer = await login(first, '111111111', 1);
        sharer.connection.closed();
        await settle();
        expect(second.onlineCount).toBe(0);
        expect(broker.online.has('222222222')).toBe(false);
        viewer.connection.receive(signal('222222222', 's1', 'offer'));
        await settle();
        expect(viewer.peer.last('undeliverable')).toBeDefined();
    });

    it('keeps the newer login of a device on another instance', async () => {
        const old = await login(first, '482915307');
        const fresh = await login(second, '482915307');
        await settle();
        expect(old.peer.last('replaced')).toEqual({ type: 'replaced' });
        expect(old.peer.closedWith).toBe(CloseCode.Replaced);
        expect(first.onlineCount).toBe(0);

        // 信令只送到新的那条
        const viewer = await login(first, '111111111', 3);
        viewer.connection.receive(signal('482915307', 's1', 'offer'));
        await settle();
        expect(fresh.peer.last('signal')?.text).toBe('offer');
    });

    it('keeps the newer login of a device on the same instance', async () => {
        const old = await login(first, '482915307');
        const fresh = await login(first, '482915307');
        await settle();
        expect(old.peer.closedWith).toBe(CloseCode.Replaced);
        expect(fresh.peer.closedWith).toBeNull();
        expect(first.onlineCount).toBe(1);

        const viewer = await login(second, '111111111', 3);
        viewer.connection.receive(signal('482915307', 's1', 'offer'));
        await settle();
        expect(fresh.peer.last('signal')).toBeDefined();
    });

    it('closes connections that send something else', async () => {
        const device = connect(first);
        device.connection.receive('{"type":"ping"}');
        await settle();
        expect(device.peer.sent).toEqual([{ type: 'rejected', reason: 'invalid' }]);
        expect(device.peer.closedWith).toBe(CloseCode.Invalid);
    });

    it('needs hello before signals', async () => {
        const device = connect(first);
        device.connection.receive(signal('222222222', 's1', 'offer'));
        await settle();
        expect(device.peer.closedWith).toBe(CloseCode.Invalid);
    });

    it('allows only one hello', async () => {
        const device = await login(first, '482915307');
        device.connection.receive(hello('482915307'));
        await settle();
        expect(device.peer.closedWith).toBe(CloseCode.Invalid);
    });

    it('closes connections that never say hello', () => {
        vi.useFakeTimers();
        const quick = hub({ ...defaultHubOptions, helloTimeoutMs: 1000 });
        const device = connect(quick);
        vi.advanceTimersByTime(999);
        expect(device.peer.closedWith).toBeNull();
        vi.advanceTimersByTime(1);
        expect(device.peer.sent).toEqual([{ type: 'rejected', reason: 'timeout' }]);
        expect(device.peer.closedWith).toBe(CloseCode.Timeout);
    });

    // ---- P2P 和中继用的服务器 ----

    it('welcomes with the relay at the host the device used, and renews its credentials', async () => {
        const ice = { secret: 'dev-secret', host: null, port: 3478, transport: 'tcp' as const, ttlS: 2 };
        const relayed = hub({ ...defaultHubOptions, ice });
        const before = Math.floor(Date.now() / 1000);
        const windows = await login(relayed, '482915307', 1, '192.168.55.173');
        const welcome = windows.peer.last('welcome');
        expect(welcome?.ice?.stun).toEqual(['192.168.55.173:3478']);
        expect(welcome?.ice?.ttl).toBe(2);
        const turn = welcome?.ice?.turn;
        expect(turn?.server).toBe('192.168.55.173:3478');
        expect(turn?.transport).toBe('tcp');
        const expires = Number(turn?.username.split(':')[0]);
        expect(expires - before).toBeGreaterThanOrEqual(2);
        expect(turn).toEqual({ server: '192.168.55.173:3478', transport: 'tcp', ...turnCredentials('dev-secret', '482915307', expires) });

        // 过了一半时效发一份新的
        await until(() => windows.peer.last('ice') !== undefined);
        expect(windows.peer.last('ice')?.ice.turn.server).toBe('192.168.55.173:3478');

        // 断了就不再发
        windows.connection.closed();
        const sent = windows.peer.sent.length;
        await new Promise((resolve) => setTimeout(resolve, 1200));
        expect(windows.peer.sent.length).toBe(sent);
    });

    // ---- 账号 ----

    it('registers an account and signs this device in', async () => {
        const device = await login(first, '111111111', 1);
        await register(device, ' Li@Example.com ', 'password1', '梨茶');
        await until(() => device.peer.last('account')?.account !== null);
        expect(device.peer.last('account')).toEqual({ type: 'account', account: { email: 'li@example.com', nickname: '梨茶' } });
        expect(devicesIn(device)).toEqual([
            { deviceId: '111111111', name: '电脑', platform: 'macOS', publicKey: publicKey(1), online: true },
        ]);
    });

    it('does not register an email twice', async () => {
        const one = await login(first, '111111111', 1);
        await register(one, 'li@example.com', 'password1', '梨茶');
        await until(() => one.peer.last('account')?.account !== null);
        const two = await login(second, '222222222', 2);
        expect(await sendCode(two, 'LI@example.com')).toEqual({ type: 'accountFailed', reason: 'emailTaken' });
        expect(mailer.mails).toHaveLength(1);
    });

    it('shows every device of an account to each of them, with who is online', async () => {
        const mac = await login(first, '111111111', 1);
        await register(mac, 'li@example.com', 'password1', '梨茶');
        await until(() => mac.peer.last('account')?.account !== null);

        const windows = await login(second, '222222222', 2);
        send(windows, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => devicesIn(mac).length === 2 && devicesIn(windows).length === 2);
        expect(devicesIn(mac).every((d) => d.online)).toBe(true);

        // 一台下线了，另一台看到
        windows.connection.closed();
        await until(() => devicesIn(mac).find((d) => d.deviceId === '222222222')?.online === false);

        // 改名也看到
        send(mac, { type: 'rename', name: '书房的 Mac' });
        await until(() => devicesIn(mac).find((d) => d.deviceId === '111111111')?.name === '书房的 Mac');
    });

    // 重装以后登录同一个账号：认出是原来那台电脑，换回原来的 ID，账号下不多出一台
    it('gives a reinstalled computer its old id back when it signs in', async () => {
        const machine = 'ab'.repeat(32);
        const old = await login(first, '111111111', 1, undefined, undefined, machine);
        await register(old, 'li@example.com', 'password1', '梨茶');
        await until(() => old.peer.last('account')?.account !== null);
        const windows = await login(second, '222222222', 2);
        send(windows, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => devicesIn(windows).length === 2);
        // 卸载了：原来的下线
        old.connection.closed();
        await settle();

        // 重装：新的 ID、新的令牌和密钥，同一个硬件编号
        const reinstalled = await login(first, '333333333', 3, undefined, undefined, machine);
        send(reinstalled, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => reinstalled.peer.last('renumbered') !== undefined);
        expect(reinstalled.peer.last('renumbered')).toEqual({ type: 'renumbered', deviceId: '111111111' });
        expect(reinstalled.peer.closedWith).toBe(CloseCode.Renumbered);
        expect(registry.devices.has('333333333')).toBe(false);
        expect(registry.devices.get('111111111')?.tokenHash).toBe(hashToken(token(3)));

        // 拿原来的 ID、新的令牌再登录：还在账号下，别的设备看到的是新的公钥
        const back = await login(first, '111111111', 3, undefined, undefined, machine);
        expect(back.peer.last('account')?.account).toEqual({ email: 'li@example.com', nickname: '梨茶' });
        await until(() => devicesIn(windows).find((d) => d.deviceId === '111111111')?.publicKey === publicKey(3));
        expect(devicesIn(windows).map((d) => d.deviceId).sort()).toEqual(['111111111', '222222222']);
    });

    // 原来的那个还在线（同一台电脑上开着两份）、硬件编号不一样、没有硬件编号：都不换
    it('keeps a new id when the old one is online or another computer', async () => {
        const machine = 'ab'.repeat(32);
        const old = await login(first, '111111111', 1, undefined, undefined, machine);
        await register(old, 'li@example.com', 'password1', '梨茶');
        await until(() => old.peer.last('account')?.account !== null);

        for (const [id, n, other] of [['333333333', 3, machine], ['444444444', 4, 'cd'.repeat(32)], ['555555555', 5, '']] as const) {
            const device = await login(first, id, n, undefined, undefined, other);
            send(device, { type: 'login', email: 'li@example.com', password: 'password1' });
            await until(() => device.peer.last('account')?.account !== null);
            expect(device.peer.last('renumbered')).toBeUndefined();
        }
        await until(() => devicesIn(old).length === 4);
    });

    it('refuses a wrong password, and locks the address and the account after too many', async () => {
        const strict = strictHub({ passwordFailuresPerAddress: 3, passwordFailuresPerAccount: 5 });
        const mac = await login(first, '111111111', 1);
        await register(mac, 'li@example.com', 'password1', '梨茶');
        await until(() => mac.peer.last('account')?.account !== null);

        const guesser = await login(strict, '222222222', 2, undefined, '198.51.100.7');
        for (let i = 0; i < 3; ++i)
            send(guesser, { type: 'login', email: 'li@example.com', password: 'wrong-guess' });
        await until(() => guesser.peer.sent.filter((m) => m.type === 'accountFailed').length === 3);
        expect(guesser.peer.last('accountFailed')).toEqual({ type: 'accountFailed', reason: 'wrongPassword' });
        // 现在连对的也不让试；换一条连接也一样
        send(guesser, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => guesser.peer.last('accountFailed')?.reason === 'tooMany');
        expect(guesser.peer.last('account')).toEqual({ type: 'account', account: null });
        const again = await login(strict, '333333333', 3, undefined, '198.51.100.7');
        send(again, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => again.peer.last('accountFailed')?.reason === 'tooMany');

        // 别的地址接着试这个账号：账号错满了也锁
        const elsewhere = await login(strict, '444444444', 4, undefined, '192.0.2.9');
        send(elsewhere, { type: 'login', email: 'li@example.com', password: 'wrong-guess' });
        send(elsewhere, { type: 'login', email: 'li@example.com', password: 'wrong-guess' });
        await until(() => elsewhere.peer.sent.filter((m) => m.type === 'accountFailed').length === 2);
        send(elsewhere, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => elsewhere.peer.last('accountFailed')?.reason === 'tooMany');
    });

    it('changes the nickname and the password, and asks for the old password', async () => {
        const mac = await login(first, '111111111', 1);
        await register(mac, 'li@example.com', 'password1', '梨茶');
        await until(() => mac.peer.last('account')?.account !== null);

        send(mac, { type: 'setNickname', nickname: '新昵称' });
        await until(() => mac.peer.last('account')?.account?.nickname === '新昵称');

        send(mac, { type: 'changePassword', oldPassword: 'not-mine', newPassword: 'password2' });
        await until(() => mac.peer.last('accountFailed')?.reason === 'wrongPassword');
        send(mac, { type: 'changePassword', oldPassword: 'password1', newPassword: 'password2' });
        await until(() => mac.peer.last('passwordChanged') !== undefined);

        const windows = await login(second, '222222222', 2);
        send(windows, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => windows.peer.last('accountFailed')?.reason === 'wrongPassword');
        send(windows, { type: 'login', email: 'li@example.com', password: 'password2' });
        await until(() => windows.peer.last('account')?.account?.nickname === '新昵称');
    });

    it('deletes the account with its password, and every device leaves it', async () => {
        const mac = await login(first, '111111111', 1);
        await register(mac, 'li@example.com', 'password1', '梨茶');
        await until(() => mac.peer.last('account')?.account !== null);
        const windows = await login(second, '222222222', 2);
        send(windows, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => devicesIn(mac).length === 2);

        send(mac, { type: 'deleteAccount', password: 'wrong-guess' });
        await until(() => mac.peer.last('accountFailed')?.reason === 'wrongPassword');
        expect(accounts.count).toBe(1);

        send(mac, { type: 'deleteAccount', password: 'password1' });
        await until(() => mac.peer.last('account')?.account === null && windows.peer.last('account')?.account === null);
        expect(accounts.count).toBe(0);
        expect(await registry.accountOf('222222222')).toBeNull();
        // 邮箱又能注册了
        await register(windows, 'li@example.com', 'password3', '又是我');
        await until(() => windows.peer.last('account')?.account?.nickname === '又是我');
    });

    it('stops a device that keeps calling devices that are not there', async () => {
        const strict = strictHub({ missesPerDevice: 3 });
        const scanner = await login(strict, '111111111', 1);
        for (let i = 0; i < 3; ++i)
            send(scanner, { type: 'signal', to: `90000000${i}`, session: `s${i}`, text: 'rcs1.x', transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
        await until(() => scanner.peer.sent.filter((m) => m.type === 'undeliverable').length === 3);
        expect(scanner.peer.last('undeliverable')?.reason).toBe('offline');

        // 现在连在线的也先不让连
        const target = await login(second, '222222222', 2, undefined, '192.0.2.9');
        send(scanner, { type: 'signal', to: '222222222', session: 's9', text: 'rcs1.x', transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
        await until(() => scanner.peer.last('undeliverable')?.reason === 'tooMany');
        expect(target.peer.last('signal')).toBeUndefined();
    });

    it('counts a connection once however many signals it sends', async () => {
        const strict = strictHub({ attemptsPerDevice: 1 });
        const mac = await login(strict, '111111111', 1);
        const windows = await login(second, '222222222', 2, undefined, '192.0.2.9');
        // 同一次连接重连了几次
        for (let i = 0; i < 3; ++i)
            send(mac, { type: 'signal', to: '222222222', session: 'same', text: `rcs1.${i}`, transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
        await until(() => windows.peer.sent.filter((m) => m.type === 'signal').length === 3);
        // 新的一次连接超过了
        send(mac, { type: 'signal', to: '222222222', session: 'other', text: 'rcs1.x', transport: 'p2p', access: 'password', sdk: '0.1.0-beta' });
        await until(() => mac.peer.last('undeliverable')?.reason === 'tooMany');
    });

    it('refuses an address that registers too many new devices', async () => {
        const strict = strictHub({ newDevicesPerAddress: 2 });
        await login(strict, '111111111', 1);
        await login(strict, '222222222', 2);
        const third = connect(strict);
        third.connection.receive(hello('333333333', 3));
        await until(() => third.peer.closedWith !== null);
        expect(third.peer.last('rejected')).toEqual({ type: 'rejected', reason: 'tooMany' });
        expect(third.peer.closedWith).toBe(CloseCode.TooMany);
        // 登记过的设备照样能上来
        await login(strict, '111111111', 1);
    });

    it('marks connected devices as still in use', async () => {
        const often = hub({ ...defaultHubOptions, seenMs: 20 });
        await login(often, '111111111', 1);
        const before = registry.devices.get('111111111')!.lastSeenAt;
        await new Promise((resolve) => setTimeout(resolve, 60));
        expect(registry.devices.get('111111111')!.lastSeenAt).toBeGreaterThan(before);
    });

    it('signs out, and removes another device from the account', async () => {
        const mac = await login(first, '111111111', 1);
        await register(mac, 'li@example.com', 'password1', '梨茶');
        await until(() => mac.peer.last('account')?.account !== null);
        const windows = await login(second, '222222222', 2);
        send(windows, { type: 'login', email: 'li@example.com', password: 'password1' });
        await until(() => devicesIn(mac).length === 2);

        send(mac, { type: 'removeDevice', deviceId: '222222222' });
        await until(() => windows.peer.last('account')?.account === null && devicesIn(mac).length === 1);

        send(mac, { type: 'logout' });
        await until(() => mac.peer.last('account')?.account === null);
        expect(await registry.accountOf('111111111')).toBeNull();
    });

    it('removes only devices of its own account', async () => {
        const mac = await login(first, '111111111', 1);
        await register(mac, 'li@example.com', 'password1', '梨茶');
        const other = await login(second, '222222222', 2);
        await register(other, 'other@example.com', 'password2', '别人');
        await until(() => mac.peer.last('account')?.account !== null && other.peer.last('account')?.account !== null);

        send(mac, { type: 'removeDevice', deviceId: '222222222' });
        await settle();
        expect(await registry.accountOf('222222222')).not.toBeNull();
    });
    // ---- 验证码 ----

    it('registers only with the code sent to the email', async () => {
        const device = await login(first, '111111111', 1);
        expect(await sendCode(device, ' Li@Example.com ')).toEqual({ type: 'codeSent', purpose: 'register' });
        const code = mailer.codeFor('li@example.com');
        expect(code).toMatch(/^\d{6}$/);
        expect(mailer.mails[0]?.text).toContain(code);
        expect(stats.counts.get('mails')).toBe(1);

        const wrong = code === '000000' ? '111111' : '000000';
        const from = device.peer.sent.length;
        send(device, { type: 'register', email: 'li@example.com', password: 'password1', nickname: '梨茶', code: wrong });
        expect(await next(device, from, 'accountFailed')).toEqual({ type: 'accountFailed', reason: 'wrongCode' });
        expect(accounts.count).toBe(0);

        send(device, { type: 'register', email: 'li@example.com', password: 'password1', nickname: '梨茶', code });
        await until(() => device.peer.last('account')?.account?.email === 'li@example.com');
    });

    it('takes a code once, and drops it after five wrong tries', async () => {
        const device = await login(first, '111111111', 1);
        await sendCode(device, 'li@example.com');
        const code = mailer.codeFor('li@example.com');
        const wrong = code === '000000' ? '111111' : '000000';
        for (let i = 0; i < 5; ++i) {
            const from = device.peer.sent.length;
            send(device, { type: 'register', email: 'li@example.com', password: 'password1', nickname: '梨茶', code: wrong });
            await next(device, from, 'accountFailed');
        }
        // 错够了次数：对的也不认了，要再发一个
        const from = device.peer.sent.length;
        send(device, { type: 'register', email: 'li@example.com', password: 'password1', nickname: '梨茶', code });
        expect(await next(device, from, 'accountFailed')).toEqual({ type: 'accountFailed', reason: 'wrongCode' });
        expect(accounts.count).toBe(0);
    });

    it('limits how often codes are sent', async () => {
        const waiting = strictHub({ codeIntervalS: 60 });
        const device = await login(waiting, '111111111', 1);
        expect((await sendCode(device, 'li@example.com')).type).toBe('codeSent');
        expect(await sendCode(device, 'li@example.com')).toEqual({ type: 'accountFailed', reason: 'tooMany' });
        expect(mailer.mails).toHaveLength(1);

        // 服务器一天发的到上限了
        const daily = strictHub({ mailsPerDay: 2 });
        const other = await login(daily, '222222222', 2, undefined, '192.0.2.9');
        expect((await sendCode(other, 'a@example.com')).type).toBe('codeSent');
        expect(await sendCode(other, 'b@example.com')).toEqual({ type: 'accountFailed', reason: 'mailLimit' });
    });

    it('says so when the mail cannot be sent', async () => {
        const device = await login(first, '111111111', 1);
        mailer.failing = true;
        expect(await sendCode(device, 'li@example.com')).toEqual({ type: 'accountFailed', reason: 'mailFailed' });
        // 管理后台看得到：没发出去了一封、为什么
        expect(stats.counts.get('mailFailures')).toBe(1);
        expect(stats.lastMailFailure).toBe('发不出去');
        const unconfigured = await login(hub(testOptions, null), '222222222', 2);
        expect(await sendCode(unconfigured, 'li@example.com')).toEqual({ type: 'accountFailed', reason: 'mailFailed' });
    });

    it('resets a forgotten password with a code and signs in', async () => {
        const mac = await login(first, '111111111', 1);
        await register(mac, 'li@example.com', 'password1', '梨茶');
        await until(() => mac.peer.last('account')?.account !== null);

        const windows = await login(second, '222222222', 2);
        expect(await sendCode(windows, 'nobody@example.com', 'resetPassword'))
            .toEqual({ type: 'accountFailed', reason: 'noAccount' });
        expect(await sendCode(windows, 'li@example.com', 'resetPassword'))
            .toEqual({ type: 'codeSent', purpose: 'resetPassword' });
        // 注册的验证码不能拿来重设密码
        const from = windows.peer.sent.length;
        send(windows, { type: 'resetPassword', email: 'li@example.com', code: '123456', password: 'password9' });
        expect(await next(windows, from, 'accountFailed')).toEqual({ type: 'accountFailed', reason: 'wrongCode' });

        await sendCode(windows, 'li@example.com', 'resetPassword');
        send(windows, { type: 'resetPassword', email: 'li@example.com', code: mailer.codeFor('li@example.com'), password: 'password9' });
        await until(() => windows.peer.last('account')?.account?.email === 'li@example.com');
        expect(await accounts.verify('li@example.com', 'password1')).toBeNull();
        expect(await accounts.verify('li@example.com', 'password9')).not.toBeNull();
    });
});
