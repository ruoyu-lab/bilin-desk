// PM2：服务端两个实例（cluster 模式，共用 127.0.0.1:3000），Nginx 在前面，实例之间经 Redis 转发消息；
// 管理后台一个（127.0.0.1:3100，docs/admin.md）；网页版的转发进程一个（127.0.0.1:3200，docs/web.md）。开发用的虚拟机用 env_development（deploy-dev.sh）
module.exports = {
    apps: [
        {
            name: 'bilin-server',
            script: 'dist/main.js',
            instances: 2,
            exec_mode: 'cluster',
            node_args: '--enable-source-maps',
            // 停止时先关掉连接、断开数据库（main.ts），给它 5 秒
            kill_timeout: 5000,
            env: {
                NODE_ENV: 'production',
                HOST: '127.0.0.1',
                PORT: '3000',
                MONGO_URL: 'mongodb://127.0.0.1:27017/bilin',
                REDIS_URL: 'redis://127.0.0.1:6379',
                // 中继（coturn）的共享密钥：setup-dev.sh 在虚拟机上生成，不进仓库。中继的地址跟着设备连服务器用的主机名
                TURN_SECRET_FILE: '/etc/bilin/turn-secret',
                // 管理后台发布的新版本（DATA_DIR/updates），程序来这里下载
                UPDATES_DIR: '/var/lib/bilin/updates',
                // 发验证码邮件（src/mail.ts）：Resend 的 API 密钥只在服务器上（Server/deploy/set-mail-key.sh 写），不进仓库
                MAIL_KEY_FILE: '/etc/bilin/mail-key',
                MAIL_FROM: '比邻 <noreply@bilindesk.com>',
                // 有新的反馈时，通知邮件里管理后台的地址
                ADMIN_URL: 'https://admin.bilindesk.com/',
            },
            // 开发用的虚拟机：不真的发邮件，验证码写进日志（pm2 logs bilin-server）
            env_development: {
                MAIL: 'log',
                ADMIN_URL: 'http://localhost:8800/admin/',
            },
        },
        {
            name: 'bilin-admin',
            script: 'dist/admin/main.js',
            instances: 1,
            exec_mode: 'fork',
            node_args: '--enable-source-maps',
            kill_timeout: 5000,
            env: {
                NODE_ENV: 'production',
                HOST: '127.0.0.1',
                PORT: '3100',
                // 网页和接口在这个路径下面（Nginx 的配置里同一个）：正式服务器上管理后台有自己的域名，在根上
                BASE_PATH: '',
                MONGO_URL: 'mongodb://127.0.0.1:27017/bilin',
                REDIS_URL: 'redis://127.0.0.1:6379',
                HEALTH_URL: 'http://127.0.0.1:3000/health',
                // 备份（backups/）放在这里，部署脚本建好
                DATA_DIR: '/var/lib/bilin',
                // 密码的哈希：Server/deploy/set-admin-password.sh 写
                ADMIN_PASSWORD_FILE: '/etc/bilin/admin-password',
                // 每天几点备份（服务器的本地时间），留几份
                BACKUP_HOUR: '4',
                BACKUP_KEEP: '14',
                // 概览里看：网站的证书什么时候到期，验证码邮件配好没有（和服务端同一个密钥文件）
                CERT_HOST: 'www.bilindesk.com',
                MAIL_KEY_FILE: '/etc/bilin/mail-key',
                MAIL_FROM: '比邻 <noreply@bilindesk.com>',
            },
            // 开发用的虚拟机：只有一个地址，管理后台在 /admin 下；http，cookie 不加 Secure；没有证书；邮件写进日志
            env_development: {
                BASE_PATH: '/admin',
                COOKIE_SECURE: 'false',
                CERT_HOST: '',
                MAIL: 'log',
            },
        },
        {
            // 网页版的转发进程：浏览器和被连的电脑必须进同一个进程才配得上对，只开一个
            name: 'bilin-relay',
            script: 'dist/relay/main.js',
            instances: 1,
            exec_mode: 'fork',
            node_args: '--enable-source-maps',
            kill_timeout: 5000,
            env: {
                NODE_ENV: 'production',
                HOST: '127.0.0.1',
                PORT: '3200',
                REDIS_URL: 'redis://127.0.0.1:6379',
            },
        },
    ],
};
