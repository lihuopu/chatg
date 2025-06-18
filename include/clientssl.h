#ifndef CLIENTSSL_H
#define CLIENTSSL_H
/**
 * 功能：OpenSSL 安全通信客户端头文件
 * 说明：
 * - 定义 SSL 连接初始化所需的类型和函数
 * - 支持服务器和客户端两种模式的 SSL 通信
 * 依赖：
 * - OpenSSL 库：提供 SSL/TLS 加密功能
 * - Linux 套接字 API：提供网络通信基础
 */

#include <stdio.h>      // 标准输入输出函数
#include <sys/socket.h> // Linux/Unix 套接字 API
#include "openssl/ssl.h"  // OpenSSL SSL/TLS 核心功能
#include "openssl/err.h"  // OpenSSL 错误处理
using namespace std;

/**
 * SSL 连接模式枚举
 * 用途：区分 SSL 连接的角色（服务器或客户端）
 * 区别：
 * - 服务器模式：需要加载证书和私钥，使用 SSL_accept() 等待连接
 * - 客户端模式：可选加载证书（双向认证），使用 SSL_connect() 发起连接
 */
enum SSL_MODE {
    SSL_MODE_SERVER,  // 服务器模式
    SSL_MODE_CLIENT   // 客户端模式
};

/**
 * 同步初始化 SSL 连接
 * 参数：
 *   - cert_path: 证书文件路径（PEM 格式）
 *   - key_path: 私钥文件路径（PEM 格式）
 *   - mode: SSL 模式（服务器/客户端）
 *   - fd: 已建立的套接字文件描述符
 * 返回值：
 *   - 成功: SSL* 指针，用于后续加密通信
 *   - 失败: NULL
 * 注意事项：
 *   - 客户端模式下 cert_path 和 key_path 可为 NULL（单向认证）
 *   - 服务器模式下必须提供有效的证书和私钥
 *   - 函数会阻塞至 SSL 握手完成
 */
SSL* sync_initialize_ssl(const char* ca_path,const char* cert_path, const char* key_path, SSL_MODE mode, int fd);
void print_ssl_error(const char* message);
void print_ssl_handshake_error(int ssl_err);
#endif  // CLIENTSSL_H