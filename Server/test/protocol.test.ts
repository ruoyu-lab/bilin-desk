import { describe, expect, it } from 'vitest';

import { MAX_SIGNAL, MAX_VERSION, parseClientMessage } from '../src/protocol.js';

const token = 'A'.repeat(43);
const publicKey = Buffer.concat([Buffer.from([0x04]), Buffer.alloc(64, 1)]).toString('base64');
const hello = { type: 'hello', deviceId: '482915307', token, publicKey, name: '书房的电脑', platform: 'macOS', version: '0.1', machine: '' };

describe('parseClientMessage', () => {
    it('accepts hello and trims the name', () => {
        expect(parseClientMessage(JSON.stringify({ ...hello, name: '  书房的电脑 ' }))).toEqual(hello);
    });

    // machine 是 sha256 的十六进制；空的：那台电脑拿不到硬件编号；不带的不是 hello
    it('accepts the machine hash and refuses anything else there', () => {
        const machine = 'ab'.repeat(32);
        expect(parseClientMessage(JSON.stringify({ ...hello, machine }))).toEqual({ ...hello, machine });
        expect(parseClientMessage(JSON.stringify({ ...hello, machine: undefined }))).toBeNull();
        for (const bad of ['AB'.repeat(32), 'ab'.repeat(31), 42])
            expect(parseClientMessage(JSON.stringify({ ...hello, machine: bad }))).toBeNull();
    });

    it('accepts rename and trims the name', () => {
        expect(parseClientMessage(JSON.stringify({ type: 'rename', name: ' 客厅电脑 ' }))).toEqual({ type: 'rename', name: '客厅电脑' });
    });

    it('accepts register and login with a lowercase email', () => {
        expect(parseClientMessage(JSON.stringify({ type: 'register', email: ' Li@Example.com', password: 'password1', nickname: ' 梨茶 ', code: '012345' })))
            .toEqual({ type: 'register', email: 'li@example.com', password: 'password1', nickname: '梨茶', code: '012345' });
        expect(parseClientMessage(JSON.stringify({ type: 'sendCode', email: 'Li@Example.com', purpose: 'resetPassword' })))
            .toEqual({ type: 'sendCode', email: 'li@example.com', purpose: 'resetPassword' });
        expect(parseClientMessage(JSON.stringify({ type: 'resetPassword', email: 'li@example.com', code: '999999', password: 'password2' })))
            .toEqual({ type: 'resetPassword', email: 'li@example.com', code: '999999', password: 'password2' });
        expect(parseClientMessage(JSON.stringify({ type: 'login', email: 'LI@example.com', password: 'x' })))
            .toEqual({ type: 'login', email: 'li@example.com', password: 'x' });
        expect(parseClientMessage(JSON.stringify({ type: 'logout' }))).toEqual({ type: 'logout' });
        expect(parseClientMessage(JSON.stringify({ type: 'removeDevice', deviceId: '731204586' })))
            .toEqual({ type: 'removeDevice', deviceId: '731204586' });
    });

    it('accepts the account changes', () => {
        expect(parseClientMessage(JSON.stringify({ type: 'setNickname', nickname: ' 新昵称 ' })))
            .toEqual({ type: 'setNickname', nickname: '新昵称' });
        expect(parseClientMessage(JSON.stringify({ type: 'changePassword', oldPassword: 'x', newPassword: 'password2' })))
            .toEqual({ type: 'changePassword', oldPassword: 'x', newPassword: 'password2' });
        expect(parseClientMessage(JSON.stringify({ type: 'deleteAccount', password: 'password1' })))
            .toEqual({ type: 'deleteAccount', password: 'password1' });
    });

    it('accepts decline', () => {
        const decline = { type: 'decline', to: '731204586', session: 's1', reason: 'permission' };
        expect(parseClientMessage(JSON.stringify(decline))).toEqual(decline);
    });

    it('accepts signal over either transport, either access', () => {
        for (const [transport, access] of [['p2p', 'password'], ['relay', 'device']]) {
            const signal = { type: 'signal', to: '731204586', session: 'abc-123_x', text: 'candidate data', transport, access, sdk: '0.1.0-beta' };
            expect(parseClientMessage(JSON.stringify(signal))).toEqual(signal);
        }
    });

    // 信令带着发信的一方的 SDK 版本；不带的不收
    it('requires the SDK version on a signal', () => {
        const signal = { type: 'signal', to: '731204586', session: 's', text: 'x', transport: 'p2p', access: 'password' };
        expect(parseClientMessage(JSON.stringify({ ...signal, sdk: '0.1.0-beta' }))).toEqual({ ...signal, sdk: '0.1.0-beta' });
        expect(parseClientMessage(JSON.stringify(signal))).toBeNull();
    });

    it.each([
        ['not json', 'hello'],
        ['array', '[]'],
        ['unknown type', JSON.stringify({ type: 'ping' })],
        ['short id', JSON.stringify({ ...hello, deviceId: '48291530' })],
        ['leading zero', JSON.stringify({ ...hello, deviceId: '082915307' })],
        ['short token', JSON.stringify({ ...hello, token: 'A'.repeat(42) })],
        ['token with padding', JSON.stringify({ ...hello, token: 'A'.repeat(42) + '=' })],
        ['blank name', JSON.stringify({ ...hello, name: '   ' })],
        ['long name', JSON.stringify({ ...hello, name: 'x'.repeat(65) })],
        ['missing platform', JSON.stringify({ ...hello, platform: undefined })],
        ['blank rename', JSON.stringify({ type: 'rename', name: ' ' })],
        ['hello without a public key', JSON.stringify({ ...hello, publicKey: undefined })],
        ['hello with a short key', JSON.stringify({ ...hello, publicKey: Buffer.alloc(33, 4).toString('base64') })],
        ['register with a short password', JSON.stringify({ type: 'register', email: 'a@b.cn', password: 'short', nickname: 'x', code: '123456' })],
        ['register with a bad email', JSON.stringify({ type: 'register', email: 'nobody', password: 'password1', nickname: 'x', code: '123456' })],
        ['register without a nickname', JSON.stringify({ type: 'register', email: 'a@b.cn', password: 'password1', nickname: ' ', code: '123456' })],
        ['register without a code', JSON.stringify({ type: 'register', email: 'a@b.cn', password: 'password1', nickname: 'x' })],
        ['register with a short code', JSON.stringify({ type: 'register', email: 'a@b.cn', password: 'password1', nickname: 'x', code: '12345' })],
        ['sendCode for something else', JSON.stringify({ type: 'sendCode', email: 'a@b.cn', purpose: 'login' })],
        ['resetPassword with a short password', JSON.stringify({ type: 'resetPassword', email: 'a@b.cn', code: '123456', password: 'short' })],
        ['remove a bad device', JSON.stringify({ type: 'removeDevice', deviceId: '12' })],
        ['blank nickname', JSON.stringify({ type: 'setNickname', nickname: '  ' })],
        ['long nickname', JSON.stringify({ type: 'setNickname', nickname: 'x'.repeat(33) })],
        ['short new password', JSON.stringify({ type: 'changePassword', oldPassword: 'password1', newPassword: 'short' })],
        ['change without the old password', JSON.stringify({ type: 'changePassword', newPassword: 'password2' })],
        ['delete without a password', JSON.stringify({ type: 'deleteAccount' })],
        ['decline without reason', JSON.stringify({ type: 'decline', to: '731204586', session: 's1' })],
        ['decline with a sentence', JSON.stringify({ type: 'decline', to: '731204586', session: 's1', reason: 'not now' })],
        ['bad session', JSON.stringify({ type: 'signal', to: '731204586', session: 'a b', text: 'x', transport: 'p2p', access: 'password' })],
        ['empty signal', JSON.stringify({ type: 'signal', to: '731204586', session: 's', text: '', transport: 'p2p', access: 'password' })],
        ['long signal', JSON.stringify({ type: 'signal', to: '731204586', session: 's', text: 'x'.repeat(MAX_SIGNAL + 1), transport: 'p2p', access: 'password' })],
        ['signal without a transport', JSON.stringify({ type: 'signal', to: '731204586', session: 's', text: 'x', access: 'password' })],
        ['signal over another transport', JSON.stringify({ type: 'signal', to: '731204586', session: 's', text: 'x', transport: 'udp', access: 'password' })],
        ['signal without an access', JSON.stringify({ type: 'signal', to: '731204586', session: 's', text: 'x', transport: 'p2p' })],
        ['signal with another access', JSON.stringify({ type: 'signal', to: '731204586', session: 's', text: 'x', transport: 'p2p', access: 'key' })],
        ['signal with a long SDK version', JSON.stringify({ type: 'signal', to: '731204586', session: 's', text: 'x', transport: 'p2p', access: 'password', sdk: 'x'.repeat(MAX_VERSION + 1) })],
    ])('rejects %s', (_, raw) => {
        expect(parseClientMessage(raw)).toBeNull();
    });
});
