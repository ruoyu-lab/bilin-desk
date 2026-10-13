import { chmod, writeFile } from 'node:fs/promises';
import { text } from 'node:stream/consumers';

import { hashPassword } from '../accounts.js';

// 设管理后台的密码（Server/deploy/set-admin-password.sh 经 SSH 调它）：从标准输入读密码，哈希写进 argv[2]，
// 只有服务端的用户能读
const file = process.argv[2];
const password = (await text(process.stdin)).replace(/\r?\n$/, '');
if (!file) {
    console.error('用法：node dist/admin/set-password.js <文件>，密码从标准输入给');
    process.exit(2);
}
if (password.length < 12) {
    console.error('密码至少 12 个字');
    process.exit(1);
}
await writeFile(file, `${await hashPassword(password)}\n`, { mode: 0o600 });
await chmod(file, 0o600);
console.log('管理后台的密码设好了');
