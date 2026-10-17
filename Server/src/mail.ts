import { readFile } from 'node:fs/promises';

import { CODE_TTL_S } from './codes.js';
import type { FeedbackSummary } from './feedback.js';
import type { CodePurpose } from './protocol.js';

// 发邮件：验证码（注册、找回密码），有新的反馈时告诉管理后台里设的邮箱
export interface Mail {
    to: string;
    subject: string;
    text: string;
    html: string;
}

export interface Mailer {
    // 没发出去时抛出
    send(mail: Mail): Promise<void>;
}

// 经 Resend（https://resend.com）的接口发。from 的域名要在 Resend 里验证过（DNS 里加它给的记录）
export class ResendMailer implements Mailer {
    readonly #key: string;
    readonly #from: string;

    constructor(key: string, from: string) {
        this.#key = key;
        this.#from = from;
    }

    async send(mail: Mail): Promise<void> {
        const response = await fetch('https://api.resend.com/emails', {
            method: 'POST',
            headers: { authorization: `Bearer ${this.#key}`, 'content-type': 'application/json' },
            body: JSON.stringify({ from: this.#from, to: [mail.to], subject: mail.subject, text: mail.text, html: mail.html }),
            signal: AbortSignal.timeout(10_000),
        });
        if (!response.ok)
            throw new Error(`Resend 回了 ${response.status}：${(await response.text()).slice(0, 300)}`);
    }
}

// 开发用的虚拟机：不真的发，写进日志（验证码在里面）
export class LogMailer implements Mailer {
    readonly #log: { info(object: object, message: string): void };

    constructor(log: { info(object: object, message: string): void }) {
        this.#log = log;
    }

    async send(mail: Mail): Promise<void> {
        this.#log.info({ to: mail.to, subject: mail.subject, text: mail.text }, '邮件（开发环境，没有真的发）');
    }
}

// 环境变量（ecosystem.config.cjs）：MAIL_KEY_FILE 是 Resend 的 API 密钥（只在服务器上，Server/deploy/set-mail-key.sh 写），
// MAIL_FROM 是发件人；MAIL=log 时写进日志（开发用的虚拟机）。都没有：null，发不了验证码
export async function loadMailer(env: NodeJS.ProcessEnv, log: { info(object: object, message: string): void }):
    Promise<Mailer | null> {
    if (env.MAIL === 'log')
        return new LogMailer(log);
    if (!env.MAIL_KEY_FILE)
        return null;
    const key = (await readFile(env.MAIL_KEY_FILE, 'utf8').catch(() => '')).trim();
    return key ? new ResendMailer(key, env.MAIL_FROM ?? '比邻 <noreply@bilindesk.com>') : null;
}

// 验证码邮件
export function codeMail(purpose: CodePurpose, to: string, code: string): Mail {
    const action = purpose === 'register' ? '注册比邻账号' : '重设比邻账号的密码';
    const minutes = CODE_TTL_S / 60;
    const text = `你的验证码是 ${code}，${minutes} 分钟内有效，用来${action}。\n\n`
        + '如果不是你本人在操作，忽略这封邮件就好。\n\n比邻 · https://www.bilindesk.com\n';
    const html = `<!doctype html><html lang="zh-CN"><body style="margin:0;padding:32px 16px;background:#f4f5f7;
font-family:-apple-system,BlinkMacSystemFont,'PingFang SC','Microsoft YaHei',sans-serif;color:#15171a">
<div style="max-width:420px;margin:0 auto;padding:32px 28px;background:#ffffff;border-radius:16px">
<div style="font-size:15px;font-weight:600">比邻</div>
<p style="margin:20px 0 8px;font-size:14px;color:#5f6670">你的验证码，用来${action}：</p>
<div style="font-size:34px;font-weight:700;letter-spacing:8px">${code}</div>
<p style="margin:16px 0 0;font-size:13px;color:#5f6670">${minutes} 分钟内有效。如果不是你本人在操作，忽略这封邮件就好。</p>
</div>
<p style="max-width:420px;margin:16px auto 0;font-size:12px;color:#9aa1ab;text-align:center">
<a href="https://www.bilindesk.com" style="color:#9aa1ab">bilindesk.com</a></p>
</body></html>`;
    return { to, subject: `${code} 是你的比邻验证码`, text, html };
}

// 测试邮件：管理后台里发一封，看验证码邮件能不能送到
export function testMail(to: string): Mail {
    const text = '这是一封测试邮件：比邻的管理后台发的，用来确认验证码邮件能送到。收到了就说明发信没问题。\n\n比邻 · https://www.bilindesk.com\n';
    const html = `<!doctype html><html lang="zh-CN"><body style="margin:0;padding:32px 16px;background:#f4f5f7;
font-family:-apple-system,BlinkMacSystemFont,'PingFang SC','Microsoft YaHei',sans-serif;color:#15171a">
<div style="max-width:420px;margin:0 auto;padding:32px 28px;background:#ffffff;border-radius:16px">
<div style="font-size:15px;font-weight:600">比邻</div>
<p style="margin:20px 0 0;font-size:14px;line-height:1.7;color:#5f6670">这是一封测试邮件：比邻的管理后台发的，用来确认验证码邮件能送到。收到了就说明发信没问题。</p>
</div>
</body></html>`;
    return { to, subject: '比邻的测试邮件', text, html };
}

function escapeHtml(text: string): string {
    return text.replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]!);
}

// 有新的反馈：告诉管理后台里设的邮箱（全文和日志在后台里看）
export function feedbackMail(to: string, feedback: FeedbackSummary, adminUrl: string): Mail {
    const from = `${feedback.deviceName}（${feedback.platform} ${feedback.version}，${feedback.system}）`;
    const contact = feedback.contact || '没留';
    const logs = feedback.logsBytes > 0 ? `带了日志（${Math.ceil(feedback.logsBytes / 1024)} KB）` : '没带日志';
    const text = `${feedback.text}\n\n来自：${from}\n联系方式：${contact}\n${logs}\n\n在管理后台里看：${adminUrl}\n`;
    const html = `<!doctype html><html lang="zh-CN"><body style="margin:0;padding:32px 16px;background:#f4f5f7;
font-family:-apple-system,BlinkMacSystemFont,'PingFang SC','Microsoft YaHei',sans-serif;color:#15171a">
<div style="max-width:560px;margin:0 auto;padding:32px 28px;background:#ffffff;border-radius:16px">
<div style="font-size:15px;font-weight:600">比邻 · 新的反馈</div>
<p style="margin:20px 0 0;font-size:14px;line-height:1.7;white-space:pre-wrap">${escapeHtml(feedback.text)}</p>
<p style="margin:20px 0 0;font-size:13px;line-height:1.8;color:#5f6670">来自：${escapeHtml(from)}<br>
联系方式：${escapeHtml(contact)}<br>${logs}</p>
<p style="margin:20px 0 0;font-size:13px"><a href="${adminUrl}" style="color:#15171a">在管理后台里看</a></p>
</div>
</body></html>`;
    const subject = `比邻的反馈：${feedback.text.replace(/\s+/g, ' ').slice(0, 40)}`;
    return { to, subject, text, html };
}
