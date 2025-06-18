#pragma once
/**
 * 功能：SSL/TLS 加密通信所需的证书和私钥声明
 * 说明：
 * - 定义了两个外部常量字符串，用于存储 CA 证书和服务器证书（PEM 格式）
 * - 实际内容需在 .cpp 文件中定义，此头文件仅作声明
 */

/**
 * CA 根证书内容（PEM 格式）
 * 用途：验证服务器证书的合法性（单向认证）或客户端证书（双向认证）
 * 示例格式：
 * -----BEGIN CERTIFICATE-----
 * MIIE...（Base64 编码内容）...
 * -----END CERTIFICATE-----
 */
extern const char* ca_cert_key_pem;
/**
 * 服务器证书与私钥（PEM 格式）
 * 用途：服务器端 SSL/TLS 通信的身份凭证
 * 包含内容：
 * - 服务器公钥证书（由 CA 签名或自签名）
 * - 服务器私钥（用于密钥交换和数据签名）
 * 示例格式：
 * -----BEGIN CERTIFICATE-----
 * MIIE...（证书内容）...
 * -----END CERTIFICATE-----
 * -----BEGIN PRIVATE KEY-----
 * MIIE...（私钥内容）...
 * -----END PRIVATE KEY-----
 */
extern const char* server_cert_key_pem;