# 比邻的管理后台：每天用 mongodump 备份，所以在 MongoDB 的镜像（自带 mongodump）里加上 Node
FROM node:24-slim AS node
FROM mongo:8.0
COPY --from=node /usr/local/bin/node /usr/local/bin/node
