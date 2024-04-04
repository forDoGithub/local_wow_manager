
/*
   void navigation_transmission(SSL*& ssl) {
        float data[6]{};
        int received = SSL_read(ssl, reinterpret_cast<char*>(data), sizeof(data));

    // this is where bot c++ has sent data notice the struct on both ends []
    //tetra/calls.ixx
    float data[6]{ start.x, start.y, start.z, end.x, end.y, end.z };

    if (SSL_write(ssl, reinterpret_cast<const char*>(data), sizeof(data)) == -1) {
        std::cerr << "Can't send data to server! Quitting" << std::endl;
        lua_pushnil(L);
        return 1;
    }

    float result[3]{};
    int received = SSL_read(ssl, reinterpret_cast<char*>(result), sizeof(result));

    if (received == sizeof(result)) {
        lua_pushnumber(L, result[0]);
        lua_pushnumber(L, result[1]);
        lua_pushnumber(L, result[2]);

        close_ssl(ssl, ctx, sock);

        return 3;
    }
*/


in game lua
C("Update_My_Data")

calls.ixx

__TIMESTAMP__ character_data_transmission(SSL*& ssl) {
	char dataBuffer[1024] = { 0 };
	__TIMESTAMP__ timestamp = readDataFromSSL(ssl, dataBuffer, sizeof(dataBuffer));
    if (timestamp != __TIMESTAMP__()) {
		handleMessage(ssl, dataBuffer, timestamp);
	}
	return timestamp;
}

hah just realized this at time of creation was repository 23 created 1.23.2023 