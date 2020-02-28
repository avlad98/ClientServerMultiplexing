CFLAGS = -Wall -g

PORT = 6666
IP_SERVER = 127.0.0.1
ID_CLIENT = "client1"


all: server subscriber

# Compileaza server.cpp
server: server.cpp

# Compileaza subscriber.cpp
subscriber: subscriber.cpp

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server ${PORT}

# Ruleaza subscriberul
run_subscriber:
	./subscriber ${ID_CLIENT} ${IP_SERVER} ${PORT}

clean:
	rm -f server subscriber
