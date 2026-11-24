import { describe, expect, it } from 'vitest';

import { summarize, type ConnectionRecord } from '../src/connections.js';

function record(change: Partial<ConnectionRecord>): ConnectionRecord {
    return {
        id: 'x', createdAt: new Date(2026, 10, 24, 12).toISOString(),
        viewer: { deviceId: '111111111', platform: 'Windows', version: '0.1.6-beta' },
        peer: { deviceId: '222222222', platform: 'macOS', version: '0.1.6-beta' },
        access: 'device', result: 'connected', reason: '', routes: ['direct'], path: 'lan', connectMs: 1000, durationS: 60,
        reconnects: 0, approval: false, control: true, codec: 'HEVC', resolution: '1920 × 1080', fps: 60, mbps: 8,
        latencyMs: 3, ...change,
    };
}

describe('summarize', () => {
    const now = new Date(2026, 10, 24, 18);

    it('counts attempts, paths, reasons and days', () => {
        const summary = summarize([
            record({}),
            record({ routes: ['direct', 'relay', 'relayTls'], path: 'relay', connectMs: 21000, durationS: 600,
                     reason: '对方断开了连接', reconnects: 2, createdAt: new Date(2026, 10, 23, 9).toISOString() }),
            record({ result: 'failed', reason: '对方不在线，或者设备 ID 不对', path: '', connectMs: -1, durationS: 0,
                     viewer: { deviceId: '333333333', platform: 'macOS', version: '0.1.5-beta' } }),
            record({ result: 'cancelled', path: '', connectMs: -1, durationS: 0 }),
            record({ peer: { deviceId: '', platform: '', version: '' }, access: 'password' }),
        ], 7, now);
        expect(summary).toMatchObject({
            attempts: 4, connected: 3, cancelled: 1, viewers: 2, connectMsMedian: 1000, durationSTotal: 720,
            paths: [{ key: 'lan', count: 2 }, { key: 'relay', count: 1 }],
            routes: [{ key: 'direct', count: 2 }, { key: 'relayTls', count: 1 }],
            failures: [{ key: '对方不在线，或者设备 ID 不对', count: 1 }],
            endings: [{ key: '对方断开了连接', count: 1 }],
            access: [{ key: 'device', count: 3 }, { key: 'lan', count: 1 }],
            versions: [{ version: '0.1.6-beta', attempts: 3, connected: 3 }, { version: '0.1.5-beta', attempts: 1, connected: 0 }],
            reconnected: 1, control: 3,
        });
        expect(summary.platforms[0]).toEqual({ key: 'Windows → macOS', count: 2 });
        expect(summary.daily).toHaveLength(7);
        expect(summary.daily.at(-1)).toEqual({ day: '2026-11-24', attempts: 3, connected: 2 });
        expect(summary.daily.at(-2)).toEqual({ day: '2026-11-23', attempts: 1, connected: 1 });
    });

    it('has no medians without connections', () => {
        expect(summarize([], 7, now)).toMatchObject({ attempts: 0, connectMsMedian: null, durationSMedian: null });
    });
});
