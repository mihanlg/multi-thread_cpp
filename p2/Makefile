all:
	g++ -std=c++1y ./chat_server/server.cpp -o chatsrv
	g++ -std=c++1y ./chat_client/client.cpp -o chatclt


test_server:
	g++ -std=c++1y ./chat_server/server.cpp -o chatsrv
	python test.py

test_client:
	g++ -std=c++1y ./chat_client/client.cpp -o chatclt

clean:
	rm -rf chatsrv
	rm -rf chatclt


