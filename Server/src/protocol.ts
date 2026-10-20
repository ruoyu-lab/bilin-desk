// 后台服务和服务器之间的消息：WebSocket 上一条文字消息是一个 JSON 对象，type 说明是哪一种。
// 客户端那一边在 Core/src/rc/server_protocol.h，改了两边一起改。
//
// 连上以后先发 hello（10 秒内），服务器回 welcome 才算登录；之后可以发 signal、decline、rename，
// 和账号的 sendCode、register、login、resetPassword、logout、removeDevice。
// 信令是 SDK 给的一段文字（候选地址和一次性凭据，不含任何秘密），服务器不看内容，按 to 转给那台设备，
// 带上发信的设备的 ID 和名字（对方的界面要显示是谁在连）；session 由发起连接的一方起，回答时原样带回，用来配对。
// 被连的一方现在不接受连接时回 decline（原因由两台设备自己约定），服务器同样转回去。
// 每份信令带着发信的一方这次用的传输方式（transport）：两台设备先直连，打不通再都换成中继（SDK 要求两端一样）；
// 和怎么证明自己（access）：别人用临时密码，同一个账号下的设备只靠设备密钥（两端也要一样）；
// 还有发信的一方的 SDK 版本（sdk）：程序 0.1.1-beta 和以前的按它回绝版本不一样的，之后的只照旧带着（SDK 握手时自己核对）。
// welcome 带着 P2P 和中继用的服务器（ice，coturn）；中继的凭据有时效，过了一半时效服务器再发一份新的（ice）。
//
// 账号：邮箱 + 密码。注册、找回密码先 sendCode，服务器往邮箱发 6 位验证码（codes.ts、mail.ts），回 codeSent；
// register、resetPassword 带着它（找回密码重设以后直接登录）。登录就是把这台设备挂到账号下（设备已经用令牌证明了自己是哪台），
// 退出就是拿下来；一个账号可以挂多台设备，一台设备同一时间一个账号。登录着的设备收到 account（账号是谁）
// 和 devices（账号下所有设备、在不在线），有变化时再收到一次。登录着的设备可以改昵称、改密码（要旧密码）、
// 注销账号（要密码：账号删掉，挂在下面的设备都不再属于它）
//
// 网络唤醒（docs/devices-and-accounts.md 的"远程唤醒"）：登录着账号的电脑把自己能唤醒的网卡告诉服务器（wakeInfo），
// 服务器存着（退出、被移出账号时删掉）。账号下的一台睡着了、关机了，账号下别的设备（wake）或者网页版要叫醒它时，
// 服务器把它的网卡转给账号下开着的别的电脑（wakeRequest），和它在同一个网段的那台从本机发唤醒包，回答发没发（wakeAnswer）；
// 服务器把结果告诉要叫醒它的一方（wakeResult）。网卡信息只给这几台电脑，账号下的设备列表里只说叫不叫得醒（wake）
//
// 限速（免得有人挨个试设备 ID、密码）：同一个地址、同一台设备发起的连接，连不上的（对方不在线、没有这个 ID），
// 同一个地址登记的新设备，同一个地址、同一个邮箱输错的密码、要的验证码，在一段时间里超过了上限就先不让做（tooMany）

export const DEVICE_ID = /^[1-9]\d{8}$/;
// 设备令牌：32 字节随机数的 base64url（不带"="）。服务器只存它的哈希
export const TOKEN = /^[A-Za-z0-9_-]{43}$/;
// 这台电脑的硬件编号的哈希（sha256 的十六进制）：重装以后登录同一个账号，认出是账号下原来的那一台
export const MACHINE = /^[0-9a-f]{64}$/;
// 设备密钥的公钥：P-256，0x04 || X || Y 共 65 字节（标准 base64）
export const PUBLIC_KEY_BYTES = 65;
export const SESSION = /^[A-Za-z0-9_-]{1,64}$/;
// 网页版的一次性通行证（tickets.ts）
export const TICKET = /^[A-Za-z0-9_-]{43}$/;
export const EMAIL = /^[^@\s]+@[^@\s]+\.[^@\s]+$/;
// 邮件里的验证码
export const CODE = /^\d{6}$/;
export const MAX_NAME = 64;
export const MAX_NICKNAME = 32;
export const MAX_EMAIL = 254;
export const MIN_PASSWORD = 8;
export const MAX_PASSWORD = 128;
export const MAX_PLATFORM = 32;
export const MAX_VERSION = 32;
export const MAX_SIGNAL = 16 * 1024;
export const REASON = /^[A-Za-z]{1,32}$/;
// p2p：直连 | relay：经中继（TURN）
export const TRANSPORTS = ['p2p', 'relay'] as const;
export type Transport = (typeof TRANSPORTS)[number];
// password：临时密码 | device：我的设备，只靠设备密钥
export const ACCESSES = ['password', 'device'] as const;
export type Access = (typeof ACCESSES)[number];
// 验证码是干什么用的：注册 | 找回密码
export const CODE_PURPOSES = ['register', 'resetPassword'] as const;
export type CodePurpose = (typeof CODE_PURPOSES)[number];

// 能唤醒一台电脑的一块网卡：MAC 地址（小写，冒号隔开）、它的 IPv4 地址和网段、网线还是 Wi-Fi、系统里开没开网络唤醒
// （unknown：系统不说）
export const WAKE_LINKS = ['ethernet', 'wifi'] as const;
export const WAKE_SETTINGS = ['enabled', 'disabled', 'unknown'] as const;
export interface WakeInterface {
    mac: string;
    address: string;
    prefix: number;
    link: (typeof WAKE_LINKS)[number];
    wake: (typeof WAKE_SETTINGS)[number];
}
// 一台电脑最多说几块（SDK 的 RCS_POWER_INTERFACES_MAX）
export const MAX_WAKE_INTERFACES = 8;
const MAC = /^[0-9a-f]{2}(:[0-9a-f]{2}){5}$/;
const IPV4 = /^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$/;
// 帮忙叫醒的电脑的回答：sent（发了唤醒包）| elsewhere（它和本机不在同一个网段）| failed（系统不让发）
export const WAKE_ANSWERS = ['sent', 'elsewhere', 'failed'] as const;
export type WakeAnswerResult = (typeof WAKE_ANSWERS)[number];
// 叫醒的结果：sent（helper 那台发了唤醒包）| online（它在线，不用叫）| noInfo（它没告诉过服务器网卡：要装上新版本、
// 登录着账号上线一次）| disabled（它的网卡都没开网络唤醒）| noHelper（账号下没有开着的、能帮忙的别的电脑）
// | elsewhere（开着的都不和它在同一个网段）| failed（帮忙的电脑系统不让发）| tooMany（叫得太频繁）| notMine（不是本账号的）
export type WakeResult = 'sent' | 'online' | 'noInfo' | 'disabled' | 'noHelper' | 'elsewhere' | 'failed' | 'tooMany' | 'notMine';
// 账号下的设备叫不叫得醒：none（没告诉过网卡）| disabled（网卡都没开网络唤醒）| ready（能请别的电脑帮忙叫）
export type WakeState = 'none' | 'disabled' | 'ready';

// ---- 客户端 → 服务器 ----

export interface Hello {
    type: 'hello';
    deviceId: string;
    token: string;
    publicKey: string;
    name: string;
    platform: string;
    version: string;
    // 这台电脑的硬件编号的哈希；拿不到时为空
    machine: string;
}

export interface SignalRequest {
    type: 'signal';
    to: string;
    session: string;
    text: string;
    transport: Transport;
    access: Access;
    sdk: string;
}

export interface DeclineRequest {
    type: 'decline';
    to: string;
    session: string;
    reason: string;
}

// 设备改了名字
export interface Rename {
    type: 'rename';
    name: string;
}

// 往邮箱发验证码：注册时邮箱还没注册过，找回密码时注册过。发了回 codeSent
export interface SendCode {
    type: 'sendCode';
    email: string;
    purpose: CodePurpose;
}

// 注册一个账号（code 是发到 email 的验证码），这台设备随即登录它
export interface Register {
    type: 'register';
    email: string;
    password: string;
    nickname: string;
    code: string;
}

export interface Login {
    type: 'login';
    email: string;
    password: string;
}

// 忘了密码：凭发到 email 的验证码设新密码，这台设备随即登录这个账号
export interface ResetPassword {
    type: 'resetPassword';
    email: string;
    code: string;
    password: string;
}

export interface Logout {
    type: 'logout';
}

// 网页版（docs/web.md）：对 webViewer 的回答。reason 空的是准备好了（进了转发进程里的房间、换好了钥匙），
// 不然是为什么不行（rc/peer_protocol.h 的原因：notAccepting、transportBusy……）
export interface WebReady {
    type: 'webReady';
    ticket: string;
    reason: string;
}

// 本机能唤醒它的网卡（空的：没有、系统不说）。登录着账号时发，变了再发
export interface WakeInfo {
    type: 'wakeInfo';
    interfaces: WakeInterface[];
}

// 叫醒账号下的设备 deviceId，服务器回 wakeResult
export interface Wake {
    type: 'wake';
    deviceId: string;
}

// 对 wakeRequest 的回答
export interface WakeAnswer {
    type: 'wakeAnswer';
    request: string;
    result: WakeAnswerResult;
}

// 把账号下的另一台设备移出账号
export interface RemoveDevice {
    type: 'removeDevice';
    deviceId: string;
}

export interface SetNickname {
    type: 'setNickname';
    nickname: string;
}

// 成功了回 passwordChanged，旧密码不对回 accountFailed
export interface ChangePassword {
    type: 'changePassword';
    oldPassword: string;
    newPassword: string;
}

// 密码对了就删掉账号：这台设备收到 account（null）
export interface DeleteAccount {
    type: 'deleteAccount';
    password: string;
}

export type ClientMessage =
    | Hello | SignalRequest | DeclineRequest | Rename | SendCode | Register | Login | ResetPassword | Logout | RemoveDevice
    | SetNickname | ChangePassword | DeleteAccount | WebReady | WakeInfo | Wake | WakeAnswer;

// ---- 服务器 → 客户端 ----

// idTaken：这个设备 ID 已经属于别的设备（令牌对不上），换一个再来 | tooMany：这个地址登记的新设备太多了，过一会儿再来
export type RejectReason = 'idTaken' | 'invalid' | 'timeout' | 'tooMany';

// emailTaken：邮箱注册过了 | noAccount：找回密码时这个邮箱没注册过 | wrongPassword：邮箱或密码不对（不说是哪个）
// | wrongCode：验证码不对、过期了、错太多次作废了 | tooMany：试错太多次、要验证码太频繁，等一会儿
// | mailLimit：服务器今天发的邮件到上限了 | mailFailed：邮件没发出去
export type AccountFailure =
    'emailTaken' | 'noAccount' | 'wrongPassword' | 'wrongCode' | 'tooMany' | 'mailLimit' | 'mailFailed';

export interface AccountInfo {
    email: string;
    nickname: string;
}

export interface AccountDevice {
    deviceId: string;
    name: string;
    platform: string;
    publicKey: string;
    online: boolean;
    wake: WakeState;
}

// P2P 和中继用的服务器（"主机:端口"）：STUN 告诉设备自己的公网地址；TURN 是中继，transport 是设备到它怎么走，
// 凭据（coturn 的 REST 凭据）还能用 ttl 秒
export type TurnTransport = 'udp' | 'tcp';

export interface IceServers {
    stun: string[];
    turn: { server: string; transport: TurnTransport; username: string; password: string };
    ttl: number;
}

export type ServerMessage =
    // ice：null 是服务器没配中继
    | { type: 'welcome'; ice: IceServers | null }
    // 中继的新凭据
    | { type: 'ice'; ice: IceServers }
    | { type: 'rejected'; reason: RejectReason }
    | {
        type: 'signal'; from: string; name: string; session: string; text: string; transport: Transport; access: Access;
        sdk: string;
    }
    | { type: 'decline'; from: string; session: string; reason: string }
    // 有浏览器（网页版）要连本机：凭 ticket 经转发进程进本机的房间（docs/web.md）。key（base64 的 32 字节）：
    // 浏览器登录了本机的账号，这次连接用这把一次性钥匙、不用临时密码（本机"我的设备"那一边，房间是 <ID>/mine）
    | { type: 'webViewer'; ticket: string; key?: string }
    // 账号下的设备 deviceId（名字 name）睡着了，请本机叫醒它：它的网卡和本机在同一个网段时发唤醒包，回 wakeAnswer。
    // by：谁要叫醒它（账号下那台设备的名字；网页版是空的），本机的连接记录里记一笔
    | { type: 'wakeRequest'; request: string; deviceId: string; name: string; by: string; interfaces: WakeInterface[] }
    // 对 wake 的回答；helper：发了唤醒包的那台的名字（sent 时）
    | { type: 'wakeResult'; deviceId: string; result: WakeResult; helper: string }
    // offline：对方不在线（或者没有这个 ID）| tooMany：发起的连接太多、连不上的太多，过一会儿再连
    | { type: 'undeliverable'; to: string; session: string; reason: 'offline' | 'tooMany' }
    // 这台设备登录的账号；null：没登录（退出了、被移出了）
    | { type: 'account'; account: AccountInfo | null }
    | { type: 'accountFailed'; reason: AccountFailure }
    // 验证码发到邮箱了
    | { type: 'codeSent'; purpose: CodePurpose }
    | { type: 'passwordChanged' }
    // 账号下所有的设备（包括这一台）
    | { type: 'devices'; devices: AccountDevice[] }
    // 这台设备从别处又登录了一次：这条连接到此为止
    | { type: 'replaced' }
    // 登录账号时认出这台电脑是账号下原来的 deviceId（重装过）：那个 ID 换成了这台设备的令牌、公钥，
    // 换回它、用同一个令牌重新登录。这条连接到此为止
    | { type: 'renumbered'; deviceId: string };

function isRecord(value: unknown): value is Record<string, unknown> {
    return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function text(value: unknown, max: number): value is string {
    return typeof value === 'string' && value.length > 0 && value.length <= max;
}

function trimmed(value: unknown): unknown {
    return typeof value === 'string' ? value.trim() : value;
}

function matches(value: unknown, pattern: RegExp): value is string {
    return typeof value === 'string' && pattern.test(value);
}

// 邮箱不分大小写：都按小写记
function email(value: unknown): string | null {
    const normalized = trimmed(value);
    if (!text(normalized, MAX_EMAIL) || !EMAIL.test(normalized))
        return null;
    return normalized.toLowerCase();
}

function isTransport(value: unknown): value is Transport {
    return TRANSPORTS.includes(value as Transport);
}

function isAccess(value: unknown): value is Access {
    return ACCESSES.includes(value as Access);
}

function isCodePurpose(value: unknown): value is CodePurpose {
    return CODE_PURPOSES.includes(value as CodePurpose);
}

function isPublicKey(value: unknown): value is string {
    if (typeof value !== 'string')
        return false;
    const bytes = Buffer.from(value, 'base64');
    return bytes.length === PUBLIC_KEY_BYTES && bytes[0] === 0x04 && bytes.toString('base64') === value;
}

function password(value: unknown): value is string {
    return typeof value === 'string' && value.length >= MIN_PASSWORD && value.length <= MAX_PASSWORD;
}

function wakeInterface(value: unknown): WakeInterface | null {
    if (!isRecord(value))
        return null;
    const { mac, address, prefix, link, wake } = value;
    if (!matches(mac, MAC) || !matches(address, IPV4) || !Number.isInteger(prefix) || (prefix as number) < 0
        || (prefix as number) > 32 || !WAKE_LINKS.includes(link as WakeInterface['link'])
        || !WAKE_SETTINGS.includes(wake as WakeInterface['wake']))
        return null;
    return { mac, address, prefix: prefix as number, link: link as WakeInterface['link'], wake: wake as WakeInterface['wake'] };
}

// 不合格的（不是 JSON 对象、不认识的 type、缺字段、格式不对、太长）返回 null
export function parseClientMessage(raw: string): ClientMessage | null {
    let message: unknown;
    try {
        message = JSON.parse(raw);
    } catch {
        return null;
    }
    if (!isRecord(message))
        return null;
    switch (message.type) {
    case 'hello': {
        const { deviceId, token, publicKey, platform, version, machine } = message;
        const name = trimmed(message.name);
        // machine 是空的：那台电脑拿不到硬件编号
        if (!matches(deviceId, DEVICE_ID) || !matches(token, TOKEN) || !isPublicKey(publicKey)
            || !text(name, MAX_NAME) || !text(platform, MAX_PLATFORM) || !text(version, MAX_VERSION)
            || (machine !== '' && !matches(machine, MACHINE)))
            return null;
        return { type: 'hello', deviceId, token, publicKey, name, platform, version, machine: machine as string };
    }
    case 'decline': {
        const { to, session, reason } = message;
        if (!matches(to, DEVICE_ID) || !matches(session, SESSION) || !matches(reason, REASON))
            return null;
        return { type: 'decline', to, session, reason };
    }
    case 'rename': {
        const name = trimmed(message.name);
        if (!text(name, MAX_NAME))
            return null;
        return { type: 'rename', name };
    }
    case 'signal': {
        const { to, session, text: signal, transport, access, sdk } = message;
        if (!matches(to, DEVICE_ID) || !matches(session, SESSION) || !text(signal, MAX_SIGNAL) || !isTransport(transport)
            || !isAccess(access) || !text(sdk, MAX_VERSION))
            return null;
        return { type: 'signal', to, session, text: signal, transport, access, sdk };
    }
    case 'sendCode': {
        const address = email(message.email);
        if (!address || !isCodePurpose(message.purpose))
            return null;
        return { type: 'sendCode', email: address, purpose: message.purpose };
    }
    case 'register': {
        const address = email(message.email);
        const nickname = trimmed(message.nickname);
        if (!address || !password(message.password) || !text(nickname, MAX_NICKNAME) || !matches(message.code, CODE))
            return null;
        return { type: 'register', email: address, password: message.password, nickname, code: message.code };
    }
    case 'login': {
        const address = email(message.email);
        if (!address || !text(message.password, MAX_PASSWORD))
            return null;
        return { type: 'login', email: address, password: message.password };
    }
    case 'resetPassword': {
        const address = email(message.email);
        if (!address || !matches(message.code, CODE) || !password(message.password))
            return null;
        return { type: 'resetPassword', email: address, code: message.code, password: message.password };
    }
    case 'logout':
        return { type: 'logout' };
    case 'removeDevice': {
        const { deviceId } = message;
        if (!matches(deviceId, DEVICE_ID))
            return null;
        return { type: 'removeDevice', deviceId };
    }
    case 'setNickname': {
        const nickname = trimmed(message.nickname);
        if (!text(nickname, MAX_NICKNAME))
            return null;
        return { type: 'setNickname', nickname };
    }
    case 'changePassword': {
        const { oldPassword, newPassword } = message;
        if (!text(oldPassword, MAX_PASSWORD) || !password(newPassword))
            return null;
        return { type: 'changePassword', oldPassword, newPassword };
    }
    case 'deleteAccount': {
        if (!text(message.password, MAX_PASSWORD))
            return null;
        return { type: 'deleteAccount', password: message.password };
    }
    case 'webReady': {
        const { ticket, reason } = message;
        if (!matches(ticket, TICKET) || (reason !== '' && !matches(reason, REASON)))
            return null;
        return { type: 'webReady', ticket, reason: reason as string };
    }
    case 'wakeInfo': {
        const { interfaces } = message;
        if (!Array.isArray(interfaces) || interfaces.length > MAX_WAKE_INTERFACES)
            return null;
        const parsed = interfaces.map(wakeInterface);
        if (parsed.some((i) => i === null))
            return null;
        return { type: 'wakeInfo', interfaces: parsed as WakeInterface[] };
    }
    case 'wake': {
        const { deviceId } = message;
        if (!matches(deviceId, DEVICE_ID))
            return null;
        return { type: 'wake', deviceId };
    }
    case 'wakeAnswer': {
        const { request, result } = message;
        if (!matches(request, TICKET) || !WAKE_ANSWERS.includes(result as WakeAnswerResult))
            return null;
        return { type: 'wakeAnswer', request, result: result as WakeAnswerResult };
    }
    default:
        return null;
    }
}
