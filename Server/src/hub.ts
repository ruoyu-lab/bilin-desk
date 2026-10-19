import { randomUUID } from 'node:crypto';

import type { Account, Accounts } from './accounts.js';
import type { Bus, BusMessage } from './bus.js';
import type { Codes } from './codes.js';
import { iceServers, type IceSettings } from './ice.js';
import type { Limiter } from './limiter.js';
import { codeMail, type Mailer } from './mail.js';
import {
    parseClientMessage, type AccountDevice, type AccountFailure, type AccountInfo, type ChangePassword,
    type DeclineRequest, type DeleteAccount, type Hello, type IceServers, type Login, type Register, type RejectReason,
    type ResetPassword, type SendCode, type ServerMessage, type SignalRequest,
} from './protocol.js';
import { hashToken, type Registry } from './registry.js';
import type { Stats } from './stats.js';
import type { Tickets } from './tickets.js';

// 一条 WebSocket 连接，Hub 只用到这些
export interface Peer {
    send(text: string): void;
    close(code: number, reason: string): void;
    // 设备连服务器时用的主机名（Host 头，不带端口）：没配 TURN_HOST 时中继的地址跟着它
    readonly host: string;
    // 设备的网络地址（Nginx 后面是它转来的真实地址）：限速按它
    readonly address: string;
}

export interface Log {
    info(object: object, message: string): void;
    warn(object: object, message: string): void;
    error(object: object, message: string): void;
}

// 关闭连接的代码（4000 起是自己的）；关之前先发 rejected / replaced 说明原因
export const CloseCode = {
    Invalid: 4000,
    IdTaken: 4001,
    Timeout: 4002,
    Replaced: 4003,
    TooMany: 4004,
    Renumbered: 4005,
    ServerError: 1011,
} as const;

// 限速：windowS 秒里最多几次（新设备按小时）。超过了先不让做，到点再说
export interface Limits {
    windowS: number;
    // 发起的连接（一次连接的第一份信令）
    attemptsPerDevice: number;
    attemptsPerAddress: number;
    // 连不上的（对方不在线、没有这个 ID）：挨个试 ID 的大多连不上
    missesPerDevice: number;
    missesPerAddress: number;
    // 输错的密码（登录、改密码、注销），按地址和按账号（邮箱）
    passwordFailuresPerAddress: number;
    passwordFailuresPerAccount: number;
    // 一小时里同一个地址登记的新设备
    newDevicesPerAddress: number;
    // 验证码：同一个邮箱隔多少秒才能再要，一小时里同一个邮箱、同一个地址最多要几次；
    // 服务器一天最多发几封（Resend 的免费额度一天 100 封）
    codeIntervalS: number;
    codesPerEmail: number;
    codesPerAddress: number;
    mailsPerDay: number;
}

// Hub 用到的数据和服务
export interface HubSources {
    registry: Registry;
    accounts: Accounts;
    bus: Bus;
    limiter: Limiter;
    codes: Codes;
    // 发验证码；null 是服务器没配（发不了，注册、找回密码都做不了）
    mailer: Mailer | null;
    // 每天的数目（管理后台看）
    stats: Stats;
    // 网页版的一次性通行证：电脑对它的回答记在这里（tickets.ts）；没有时网页版连不了
    tickets?: Tickets;
}

export interface HubOptions {
    helloTimeoutMs: number;
    // 隔多久报一次本实例上哪些设备在线（要比 bus 的 PRESENCE_TTL_S 短）
    presenceMs: number;
    // 隔多久记一次连着的设备还在用（registry 的 seen：太久没上线的设备记录会被删掉）
    seenMs: number;
    limits: Limits;
    // P2P 和中继用的服务器（ice.ts）；null：没配，不发
    ice: IceSettings | null;
}

export const defaultHubOptions: HubOptions = {
    helloTimeoutMs: 10_000,
    presenceMs: 30_000,
    seenMs: 6 * 60 * 60 * 1000,
    limits: {
        windowS: 600,
        attemptsPerDevice: 30,
        attemptsPerAddress: 100,
        missesPerDevice: 10,
        missesPerAddress: 30,
        passwordFailuresPerAddress: 10,
        passwordFailuresPerAccount: 10,
        newDevicesPerAddress: 20,
        codeIntervalS: 60,
        codesPerEmail: 5,
        codesPerAddress: 10,
        mailsPerDay: 90,
    },
    ice: null,
};

const HOUR_S = 60 * 60;
const DAY_S = 24 * HOUR_S;
// 一条连接记着最近几次发起的连接
const MAX_ATTEMPTS = 32;

// 一台设备现在登录的账号，和账号下的设备
interface AccountView {
    account: AccountInfo | null;
    devices: AccountDevice[];
}

// 连在这个实例上的设备。登录（hello）以后订阅它在 Bus 上的频道：发给它的信令从那里来，
// 它在别处又登录了、它的账号有了变化也从那里知道。一台设备同时只有一条连接，后登录的那条留下
export class Hub {
    readonly #registry: Registry;
    readonly #accounts: Accounts;
    readonly #bus: Bus;
    readonly #limiter: Limiter;
    readonly #codes: Codes;
    readonly #mailer: Mailer | null;
    readonly #stats: Stats;
    readonly #tickets: Tickets | undefined;
    readonly #log: Log;
    readonly #options: HubOptions;
    readonly #devices = new Map<string, Connection>();
    readonly #presence: NodeJS.Timeout;
    readonly #seen: NodeJS.Timeout;

    constructor(sources: HubSources, log: Log, options: HubOptions = defaultHubOptions) {
        const { registry, bus } = sources;
        this.#registry = registry;
        this.#accounts = sources.accounts;
        this.#bus = bus;
        this.#limiter = sources.limiter;
        this.#codes = sources.codes;
        this.#mailer = sources.mailer;
        this.#stats = sources.stats;
        this.#tickets = sources.tickets;
        this.#log = log;
        this.#options = options;
        bus.onMessage((deviceId, message) => this.#deliver(deviceId, message));
        this.#presence = setInterval(() => void this.#reportPresence(), options.presenceMs);
        this.#seen = setInterval(() => void this.#reportSeen(), options.seenMs);
    }

    connect(peer: Peer): Connection {
        return new Connection(this, peer, this.#log, this.#options);
    }

    get onlineCount(): number {
        return this.#devices.size;
    }

    close(): void {
        clearInterval(this.#presence);
        clearInterval(this.#seen);
    }

    // ---- Connection 用的 ----

    async login(connection: Connection, address: string, hello: Hello): Promise<boolean> {
        // 一个地址登记一大堆新设备：多半是在换着身份挨个试
        if (!await this.#registry.exists(hello.deviceId)
            && await this.#limiter.hit(`new:${address}`, HOUR_S) > this.#options.limits.newDevicesPerAddress) {
            this.#log.warn({ address }, '这个地址登记的新设备太多了');
            connection.reject('tooMany', CloseCode.TooMany);
            return false;
        }
        const tokenHash = hashToken(hello.token);
        const claim = await this.#registry.claim({
            id: hello.deviceId, tokenHash, name: hello.name, platform: hello.platform, version: hello.version,
            machine: hello.machine, publicKey: hello.publicKey,
        });
        if (claim === 'taken') {
            this.#log.warn({ deviceId: hello.deviceId }, '设备 ID 已经属于别的设备');
            connection.reject('idTaken', CloseCode.IdTaken);
            return false;
        }
        if (connection.isClosed)
            return false;
        // 别的实例上它的旧连接先关掉，再接过频道
        await this.#bus.publish(hello.deviceId, { type: 'replaced', connection: connection.id });
        const previous = this.#devices.get(hello.deviceId);
        this.#devices.set(hello.deviceId, connection);
        if (previous)
            previous.replace();
        else
            await this.#bus.subscribe(hello.deviceId);
        await this.#bus.markOnline([hello.deviceId]);
        this.#log.info({ deviceId: hello.deviceId, platform: hello.platform, version: hello.version }, '设备上线');
        return true;
    }

    // 刚登录上：告诉它自己的账号；账号下的别的设备看到它上线了
    async welcomed(deviceId: string): Promise<void> {
        const accountId = await this.#registry.accountOf(deviceId);
        if (accountId)
            await this.#notifyAccount(accountId);
        else
            await this.#notifyDevice(deviceId);
    }

    // 设备 deviceId（在 address）要发起一次连接：发起的、连不上的太多了就先不让
    async mayAttempt(deviceId: string, address: string): Promise<boolean> {
        const limits = this.#options.limits;
        const [device, place, deviceMisses, placeMisses] = await Promise.all([
            this.#limiter.hit(`attempt:device:${deviceId}`, limits.windowS),
            this.#limiter.hit(`attempt:address:${address}`, limits.windowS),
            this.#limiter.count(`miss:device:${deviceId}`),
            this.#limiter.count(`miss:address:${address}`),
        ]);
        const allowed = device <= limits.attemptsPerDevice && place <= limits.attemptsPerAddress
            && deviceMisses < limits.missesPerDevice && placeMisses < limits.missesPerAddress;
        if (!allowed)
            this.#log.warn({ deviceId, address }, '发起的连接太多、连不上的太多，先不让连');
        return allowed;
    }

    // 发起了一次连接（管理后台的每天的数目）
    async connectionStarted(): Promise<void> {
        await this.#stats.count('connections');
    }

    // 发起的连接连不上（对方不在线、没有这个 ID）
    async missed(deviceId: string, address: string): Promise<void> {
        const windowS = this.#options.limits.windowS;
        await Promise.all([
            this.#limiter.hit(`miss:device:${deviceId}`, windowS),
            this.#limiter.hit(`miss:address:${address}`, windowS),
        ]);
    }

    // 这个地址、这个账号（key：邮箱或账号 ID）输错的密码太多了
    async passwordLocked(address: string, key: string): Promise<boolean> {
        const limits = this.#options.limits;
        const [place, account] = await Promise.all([
            this.#limiter.count(`password:address:${address}`),
            this.#limiter.count(`password:account:${key}`),
        ]);
        return place >= limits.passwordFailuresPerAddress || account >= limits.passwordFailuresPerAccount;
    }

    async passwordFailed(address: string, key: string): Promise<void> {
        const windowS = this.#options.limits.windowS;
        await Promise.all([
            this.#limiter.hit(`password:address:${address}`, windowS),
            this.#limiter.hit(`password:account:${key}`, windowS),
        ]);
    }

    async signal(from: string, name: string, request: SignalRequest): Promise<boolean> {
        const receivers = await this.#bus.publish(request.to, {
            type: 'signal', from, name, session: request.session, text: request.text, transport: request.transport,
            access: request.access, sdk: request.sdk,
        });
        return receivers > 0;
    }

    // 回绝送不到（对方不在线了）就算了：它自己会等不到回答
    async decline(from: string, request: DeclineRequest): Promise<void> {
        await this.#bus.publish(request.to, { type: 'decline', from, session: request.session, reason: request.reason });
    }

    async rename(deviceId: string, name: string): Promise<void> {
        await this.#registry.rename(deviceId, name);
        this.#log.info({ deviceId, name }, '设备改名');
        const accountId = await this.#registry.accountOf(deviceId);
        if (accountId)
            await this.#notifyAccount(accountId);
    }

    // 往邮箱发验证码：注册时邮箱不能注册过，找回密码时要注册过。按邮箱、按地址限速，服务器一天发的也有上限
    async sendCode(deviceId: string, address: string, request: SendCode): Promise<AccountFailure | null> {
        const { email, purpose } = request;
        const registered = await this.#accounts.byEmail(email) !== null;
        if (purpose === 'register' && registered)
            return 'emailTaken';
        if (purpose === 'resetPassword' && !registered)
            return 'noAccount';
        if (!this.#mailer) {
            this.#log.warn({ deviceId }, '要验证码，可是服务器没配发邮件（MAIL_KEY_FILE）');
            return 'mailFailed';
        }
        const limits = this.#options.limits;
        const [recent, hourly, place] = await Promise.all([
            this.#limiter.hit(`code:recent:${email}`, limits.codeIntervalS),
            this.#limiter.hit(`code:email:${email}`, HOUR_S),
            this.#limiter.hit(`code:address:${address}`, HOUR_S),
        ]);
        if (recent > 1 || hourly > limits.codesPerEmail || place > limits.codesPerAddress)
            return 'tooMany';
        if (await this.#limiter.hit('code:day', DAY_S) > limits.mailsPerDay) {
            this.#log.warn({ deviceId }, '今天发的验证码到上限了');
            return 'mailLimit';
        }
        const code = await this.#codes.issue(purpose, email);
        try {
            await this.#mailer.send(codeMail(purpose, email, code));
        } catch (error) {
            this.#log.error({ err: error, deviceId }, '验证码邮件没发出去');
            await this.#stats.mailFailed(error instanceof Error ? error.message : String(error));
            return 'mailFailed';
        }
        await this.#stats.count('mails');
        this.#log.info({ deviceId, purpose }, '发了验证码');
        return null;
    }

    // 验证码对了才注册，再登录；验证码不对、邮箱注册过时返回失败的原因
    async register(deviceId: string, request: Register): Promise<AccountFailure | null> {
        const account = await this.createAccount(request);
        if (typeof account === 'string')
            return account;
        this.#log.info({ deviceId, accountId: account.id }, '注册了账号');
        await this.#bind(deviceId, account.id);
        return null;
    }

    // 验证码对了才建账号（客户端、网页版都是）
    async createAccount(request: Register): Promise<Account | AccountFailure> {
        if (!await this.#codes.check('register', request.email, request.code))
            return 'wrongCode';
        const account = await this.#accounts.create(request.email, request.nickname, request.password);
        return account === 'emailTaken' ? 'emailTaken' : account;
    }

    // ---- 网页版（docs/web.md）：浏览器登录账号，不是设备 ----

    // 输错的密码和客户端一样按地址、按邮箱数
    async webSignIn(address: string, request: Login): Promise<Account | AccountFailure> {
        if (await this.passwordLocked(address, request.email))
            return 'tooMany';
        const account = await this.#accounts.verify(request.email, request.password);
        if (account)
            return account;
        await this.passwordFailed(address, request.email);
        return 'wrongPassword';
    }

    async webResetPassword(request: ResetPassword): Promise<Account | AccountFailure> {
        if (!await this.#codes.check('resetPassword', request.email, request.code))
            return 'wrongCode';
        const account = await this.#accounts.byEmail(request.email);
        if (!account)
            return 'noAccount';
        await this.#accounts.setPassword(account.id, request.password);
        this.#log.info({ accountId: account.id }, '网页版凭验证码重设了密码');
        return account;
    }

    // 账号和它下面的设备（在不在线）；账号注销了：null
    async webView(accountId: string): Promise<AccountView | null> {
        const account = await this.#accounts.get(accountId);
        if (!account)
            return null;
        const devices = await this.#registry.devicesOf(accountId);
        const online = await this.#bus.online(devices.map((d) => d.id));
        return {
            account: { email: account.email, nickname: account.nickname },
            devices: devices.map((d) => ({
                deviceId: d.id, name: d.name, platform: d.platform, publicKey: d.publicKey, online: online.has(d.id),
            })),
        };
    }

    // 电脑对网页版那张通行证的回答（protocol.ts 的 WebReady）
    async webReady(ticket: string, reason: string): Promise<void> {
        await this.#tickets?.answer(ticket, reason);
    }

    // 有浏览器要连设备 to：凭 ticket 进它的房间；key 是登录了同一个账号的浏览器这次连接用的一次性钥匙（base64），
    // 别人的浏览器没有。不在线时 false
    async webViewer(to: string, ticket: string, key?: string): Promise<boolean> {
        return await this.#bus.publish(to, { type: 'webViewer', ticket, ...(key ? { key } : {}) }) > 0;
    }

    // 登录。这台电脑重装过、账号下还有它原来的 ID 时（同一个硬件编号、同一个系统、现在不在线），原来的 ID
    // 换成这台设备的令牌、公钥，这台设备换回那个 ID（返回它），账号下不会多出一台用不上的旧设备。
    // 要密码对了才认：只凭硬件编号接不过去。原来的那个在线时不认（同一台电脑上同时开着两份，比如开发时）
    async signIn(deviceId: string, request: Login): Promise<{ signedIn: boolean; renumbered?: string }> {
        const account = await this.#accounts.verify(request.email, request.password);
        if (!account)
            return { signedIn: false };
        return { signedIn: true, ...await this.#enter(deviceId, account) };
    }

    // 忘了密码：验证码对了就设新密码，再登录（同 signIn，也会认出重装过的电脑）
    async resetPassword(deviceId: string, request: ResetPassword): Promise<{ failure?: AccountFailure; renumbered?: string }> {
        if (!await this.#codes.check('resetPassword', request.email, request.code))
            return { failure: 'wrongCode' };
        const account = await this.#accounts.byEmail(request.email);
        if (!account)
            return { failure: 'noAccount' };
        await this.#accounts.setPassword(account.id, request.password);
        this.#log.info({ deviceId, accountId: account.id }, '凭验证码重设了密码');
        return this.#enter(deviceId, account);
    }

    // 把这台设备挂到账号下；是账号下原来的那台电脑时换回原来的 ID（返回它）
    async #enter(deviceId: string, account: Account): Promise<{ renumbered?: string }> {
        const previous = await this.#previousInstall(deviceId, account.id);
        if (previous) {
            await this.#registry.adopt(previous, deviceId);
            this.#log.info({ deviceId, previous, accountId: account.id }, '登录了账号：是账号下原来的那台电脑，换回原来的 ID');
            await this.#notifyAccount(account.id);
            return { renumbered: previous };
        }
        this.#log.info({ deviceId, accountId: account.id }, '登录了账号');
        await this.#bind(deviceId, account.id);
        return {};
    }

    async #previousInstall(deviceId: string, accountId: string): Promise<string | null> {
        const devices = await this.#registry.devicesOf(accountId);
        const me = await this.#registry.device(deviceId);
        if (!me?.machine)
            return null;
        const candidates = devices.filter((d) => d.id !== deviceId && d.machine === me.machine && d.platform === me.platform);
        const online = await this.#bus.online(candidates.map((d) => d.id));
        return candidates.find((d) => !online.has(d.id))?.id ?? null;
    }

    async signOut(deviceId: string): Promise<void> {
        this.#log.info({ deviceId }, '退出了账号');
        await this.#bind(deviceId, null);
    }

    // 改登录着的账号的昵称
    async setNickname(deviceId: string, nickname: string): Promise<void> {
        const accountId = await this.#registry.accountOf(deviceId);
        if (!accountId)
            return;
        await this.#accounts.setNickname(accountId, nickname);
        this.#log.info({ deviceId, accountId }, '改了昵称');
        await this.#notifyAccount(accountId);
    }

    // 设备登录着的账号；没登录时 null
    accountOf(deviceId: string): Promise<string | null> {
        return this.#registry.accountOf(deviceId);
    }

    checkPassword(accountId: string, password: string): Promise<boolean> {
        return this.#accounts.checkPassword(accountId, password);
    }

    async setPassword(deviceId: string, accountId: string, password: string): Promise<void> {
        await this.#accounts.setPassword(accountId, password);
        this.#log.info({ deviceId, accountId }, '改了密码');
    }

    // 注销：账号删掉，挂在下面的设备都拿下来，它们都知道自己不在账号里了
    async deleteAccount(deviceId: string, accountId: string): Promise<void> {
        const devices = await this.#registry.devicesOf(accountId);
        await this.#registry.unbindAll(accountId);
        await this.#accounts.remove(accountId);
        this.#log.info({ deviceId, accountId }, '注销了账号');
        await Promise.all(devices.map((d) => this.#notifyDevice(d.id)));
    }

    // 把同一个账号下的另一台设备移出账号
    async removeDevice(deviceId: string, target: string): Promise<void> {
        const accountId = await this.#registry.accountOf(deviceId);
        if (!accountId || await this.#registry.accountOf(target) !== accountId)
            return;
        this.#log.info({ deviceId, target }, '把设备移出了账号');
        await this.#bind(target, null);
    }

    // 现在登录的账号、账号下的设备
    async view(deviceId: string): Promise<AccountView> {
        const accountId = await this.#registry.accountOf(deviceId);
        return (accountId ? await this.webView(accountId) : null) ?? { account: null, devices: [] };
    }

    async logout(connection: Connection, deviceId: string): Promise<void> {
        if (this.#devices.get(deviceId) !== connection)
            return;
        this.#devices.delete(deviceId);
        this.#log.info({ deviceId }, '设备下线');
        await this.#bus.unsubscribe(deviceId);
        await this.#bus.markOffline(deviceId);
        const accountId = await this.#registry.accountOf(deviceId);
        if (accountId)
            await this.#notifyAccount(accountId);
    }

    // 换账号：原来的账号、新的账号下的设备，和它自己，都重新看一遍
    async #bind(deviceId: string, accountId: string | null): Promise<void> {
        const previous = await this.#registry.accountOf(deviceId);
        await this.#registry.bind(deviceId, accountId);
        if (previous && previous !== accountId)
            await this.#notifyAccount(previous);
        if (accountId)
            await this.#notifyAccount(accountId);
        else
            await this.#notifyDevice(deviceId);
    }

    async #notifyAccount(accountId: string): Promise<void> {
        const devices = await this.#registry.devicesOf(accountId);
        await Promise.all(devices.map((d) => this.#notifyDevice(d.id)));
    }

    async #notifyDevice(deviceId: string): Promise<void> {
        await this.#bus.publish(deviceId, { type: 'accountChanged' });
    }

    async #reportPresence(): Promise<void> {
        try {
            await this.#bus.markOnline([...this.#devices.keys()]);
        } catch (error) {
            this.#log.error({ err: error }, '报告在线的设备时出错');
        }
    }

    async #reportSeen(): Promise<void> {
        try {
            await this.#registry.seen([...this.#devices.keys()]);
        } catch (error) {
            this.#log.error({ err: error }, '记连着的设备时出错');
        }
    }

    #deliver(deviceId: string, message: BusMessage): void {
        const connection = this.#devices.get(deviceId);
        if (!connection)
            return;
        switch (message.type) {
        case 'signal':
            connection.signalled(message.from, message.session);
            connection.send({
                type: 'signal', from: message.from, name: message.name, session: message.session, text: message.text,
                transport: message.transport, access: message.access, sdk: message.sdk,
            });
            break;
        case 'decline':
            connection.send({ type: 'decline', from: message.from, session: message.session, reason: message.reason });
            break;
        case 'webViewer':
            connection.send({ type: 'webViewer', ticket: message.ticket, ...(message.key ? { key: message.key } : {}) });
            break;
        case 'accountChanged':
            connection.refreshAccount();
            break;
        case 'replaced':
            if (message.connection !== connection.id)
                connection.replace();
            break;
        }
    }
}

export class Connection {
    readonly id = randomUUID();
    readonly #hub: Hub;
    readonly #peer: Peer;
    readonly #log: Log;
    readonly #options: HubOptions;
    readonly #helloTimer: NodeJS.Timeout;
    #iceTimer: NodeJS.Timeout | undefined;
    #deviceId: string | null = null;
    #name = '';
    #loggingIn = false;
    #closed = false;
    // 最近发起过的连接（对方/session）：同一次连接的后续信令（重连）不再算一次发起
    readonly #attempts = new Set<string>();
    // 别的设备发来过信令的连接（"设备/session"）：本机回它的信令是在回答，不是发起
    readonly #answering = new Set<string>();
    // 消息一条一条处理：登录要等数据库，期间到的信令排在后面
    #queue: Promise<void> = Promise.resolve();

    constructor(hub: Hub, peer: Peer, log: Log, options: HubOptions) {
        this.#hub = hub;
        this.#peer = peer;
        this.#log = log;
        this.#options = options;
        this.#helloTimer = setTimeout(() => this.reject('timeout', CloseCode.Timeout), options.helloTimeoutMs);
    }

    get isClosed(): boolean {
        return this.#closed;
    }

    receive(raw: string): void {
        this.#enqueue(() => this.#handle(raw));
    }

    // 账号有了变化：把现在的账号、账号下的设备发给它
    refreshAccount(): void {
        this.#enqueue(async () => {
            if (!this.#deviceId)
                return;
            const view = await this.#hub.view(this.#deviceId);
            this.send({ type: 'account', account: view.account });
            if (view.account)
                this.send({ type: 'devices', devices: view.devices });
        });
    }

    // 对方已经断开
    closed(): void {
        if (this.#closed)
            return;
        this.#closed = true;
        clearTimeout(this.#helloTimer);
        clearTimeout(this.#iceTimer);
        if (this.#deviceId)
            void this.#hub.logout(this, this.#deviceId);
    }

    send(message: ServerMessage): void {
        if (!this.#closed)
            this.#peer.send(JSON.stringify(message));
    }

    reject(reason: RejectReason, code: number): void {
        this.send({ type: 'rejected', reason });
        this.#close(code, reason);
    }

    replace(): void {
        this.send({ type: 'replaced' });
        this.#close(CloseCode.Replaced, 'replaced');
    }

    #enqueue(work: () => Promise<void>): void {
        this.#queue = this.#queue.then(work).catch((error: unknown) => {
            // 数据库、Redis 出错：断开，客户端会重连
            this.#log.error({ err: error, deviceId: this.#deviceId }, '处理消息时出错');
            this.#close(CloseCode.ServerError, 'server error');
        });
    }

    async #handle(raw: string): Promise<void> {
        if (this.#closed)
            return;
        const message = parseClientMessage(raw);
        if (!message) {
            this.reject('invalid', CloseCode.Invalid);
            return;
        }
        if (message.type === 'hello') {
            if (this.#deviceId || this.#loggingIn) {
                this.reject('invalid', CloseCode.Invalid);
                return;
            }
            this.#loggingIn = true;
            clearTimeout(this.#helloTimer);
            if (await this.#hub.login(this, this.#peer.address, message)) {
                this.#deviceId = message.deviceId;
                this.#name = message.name;
                const settings = this.#options.ice;
                this.send({ type: 'welcome', ice: settings ? this.#ice(message.deviceId, settings) : null });
                await this.#hub.welcomed(message.deviceId);
            }
            return;
        }
        const deviceId = this.#deviceId;
        if (!deviceId) {
            this.reject('invalid', CloseCode.Invalid);
            return;
        }
        switch (message.type) {
        case 'rename':
            this.#name = message.name;
            await this.#hub.rename(deviceId, message.name);
            return;
        case 'decline':
            await this.#hub.decline(deviceId, message);
            return;
        case 'signal':
            await this.#signal(deviceId, message);
            return;
        case 'sendCode': {
            const failure = await this.#hub.sendCode(deviceId, this.#peer.address, message);
            this.send(failure ? { type: 'accountFailed', reason: failure } : { type: 'codeSent', purpose: message.purpose });
            return;
        }
        case 'register': {
            const failure = await this.#hub.register(deviceId, message);
            if (failure)
                this.send({ type: 'accountFailed', reason: failure });
            return;
        }
        case 'login':
            await this.#login(deviceId, message);
            return;
        case 'resetPassword': {
            const result = await this.#hub.resetPassword(deviceId, message);
            if (result.failure)
                this.send({ type: 'accountFailed', reason: result.failure });
            else if (result.renumbered)
                this.#renumbered(result.renumbered);
            return;
        }
        case 'logout':
            await this.#hub.signOut(deviceId);
            return;
        case 'removeDevice':
            await this.#hub.removeDevice(deviceId, message.deviceId);
            return;
        case 'setNickname':
            await this.#hub.setNickname(deviceId, message.nickname);
            return;
        case 'changePassword':
            await this.#changePassword(deviceId, message);
            return;
        case 'deleteAccount':
            await this.#deleteAccount(deviceId, message);
            return;
        case 'webReady':
            await this.#hub.webReady(message.ticket, message.reason);
            return;
        }
    }

    // 设备 from 在连接 session 里发来了信令
    signalled(from: string, session: string): void {
        if (this.#answering.size >= MAX_ATTEMPTS)
            this.#answering.delete(this.#answering.values().next().value!);
        this.#answering.add(`${from}/${session}`);
    }

    async #signal(deviceId: string, message: SignalRequest): Promise<void> {
        const attempt = `${message.to}/${message.session}`;
        if (!this.#attempts.has(attempt)) {
            if (!await this.#hub.mayAttempt(deviceId, this.#peer.address)) {
                this.send({ type: 'undeliverable', to: message.to, session: message.session, reason: 'tooMany' });
                return;
            }
            // 只记最近的几次：同一次连接的信令都挨着来
            if (this.#attempts.size >= MAX_ATTEMPTS)
                this.#attempts.delete(this.#attempts.values().next().value!);
            this.#attempts.add(attempt);
            if (!this.#answering.has(attempt))
                await this.#hub.connectionStarted();
        }
        if (await this.#hub.signal(deviceId, this.#name, message))
            return;
        await this.#hub.missed(deviceId, this.#peer.address);
        this.send({ type: 'undeliverable', to: message.to, session: message.session, reason: 'offline' });
    }

    // P2P 和中继用的服务器。中继的凭据有时效：过了一半时效再发一份新的，设备手里的总还能用一阵
    #ice(deviceId: string, settings: IceSettings): IceServers {
        this.#iceTimer = setTimeout(() => this.send({ type: 'ice', ice: this.#ice(deviceId, settings) }), settings.ttlS * 500);
        return iceServers(settings, deviceId, this.#peer.host, Date.now());
    }

    // 输错的密码按地址、按邮箱数：免得有人一个个试（换一条连接也一样）
    async #login(deviceId: string, request: Login): Promise<void> {
        const address = this.#peer.address;
        if (await this.#hub.passwordLocked(address, request.email)) {
            this.send({ type: 'accountFailed', reason: 'tooMany' });
            return;
        }
        const result = await this.#hub.signIn(deviceId, request);
        if (result.renumbered) {
            this.#renumbered(result.renumbered);
            return;
        }
        if (result.signedIn)
            return;
        await this.#hub.passwordFailed(address, request.email);
        this.send({ type: 'accountFailed', reason: 'wrongPassword' });
    }

    // 认出是账号下原来的那台电脑：换回原来的 ID 重新登录，这条连接到此为止
    #renumbered(deviceId: string): void {
        this.send({ type: 'renumbered', deviceId });
        this.#close(CloseCode.Renumbered, 'renumbered');
    }

    // 改密码、注销账号先核对登录着的账号的密码；没登录的不理
    async #verifyPassword(deviceId: string, password: string): Promise<string | null> {
        const address = this.#peer.address;
        const accountId = await this.#hub.accountOf(deviceId);
        if (!accountId)
            return null;
        if (await this.#hub.passwordLocked(address, accountId)) {
            this.send({ type: 'accountFailed', reason: 'tooMany' });
            return null;
        }
        if (await this.#hub.checkPassword(accountId, password))
            return accountId;
        await this.#hub.passwordFailed(address, accountId);
        this.send({ type: 'accountFailed', reason: 'wrongPassword' });
        return null;
    }

    async #changePassword(deviceId: string, request: ChangePassword): Promise<void> {
        const accountId = await this.#verifyPassword(deviceId, request.oldPassword);
        if (!accountId)
            return;
        await this.#hub.setPassword(deviceId, accountId, request.newPassword);
        this.send({ type: 'passwordChanged' });
    }

    async #deleteAccount(deviceId: string, request: DeleteAccount): Promise<void> {
        const accountId = await this.#verifyPassword(deviceId, request.password);
        if (accountId)
            await this.#hub.deleteAccount(deviceId, accountId);
    }

    #close(code: number, reason: string): void {
        if (this.#closed)
            return;
        this.#peer.close(code, reason);
        this.closed();
    }
}
