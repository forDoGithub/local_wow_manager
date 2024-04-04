//#include <iostream>
//#include <winsock2.h>
//#include <openssl/ssl.h>
//#include <openssl/err.h>
//#include <filesystem>
#include "framework.h"
export module socket_utils;

export int create_and_bind_socket(int port)
{
	// Create and bind the socket
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd == INVALID_SOCKET)
	{
		std::cerr << "Can't create socket! Quitting" << std::endl;
		WSACleanup();
		return -1;
	}
	sockaddr_in hint{};
	hint.sin_family = AF_INET;
	hint.sin_port = htons(port);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(socket_fd, (sockaddr*)&hint, sizeof(hint)) == SOCKET_ERROR)
	{
		std::cerr << "Can't bind socket! Quitting" << std::endl;
		closesocket(socket_fd);
		WSACleanup();
		return -1;
	}
	return socket_fd;
}

export SSL_CTX* init_server_ctx()
{
	const SSL_METHOD* method;
	SSL_CTX* ctx;

	OpenSSL_add_all_algorithms();
	SSL_load_error_strings();

	method = TLS_server_method();
	ctx = SSL_CTX_new(method);
	if (!ctx)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}

	return ctx;
}

export void load_certificates(SSL_CTX* ctx, const char* cert_file, const char* key_file)
{
	// WARNING: Make sure paths are correct for production/local
	std::filesystem::path source_path(__FILE__);
	std::filesystem::path parent_path = source_path.parent_path();
	std::filesystem::path ssl_dir = parent_path.parent_path().append("ssl\\");

	auto cert_path = ssl_dir.string() + cert_file;
	auto key_path = ssl_dir.string() + key_file;

	if (SSL_CTX_use_certificate_file(ctx, cert_path.c_str(), SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, key_path.c_str(), SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_fp(stderr);
		abort();
	}

	if (!SSL_CTX_check_private_key(ctx))
	{
		fprintf(stderr, "Private key does not match the public certificate\n");
		abort();
	}
}