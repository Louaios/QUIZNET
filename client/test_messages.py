#test_messages.py

#!/usr/bin/env python3
import socket
import json
import time

def test_session():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', 5556))
    
    # Login
    msg = "POST player/login\n" + json.dumps({"pseudo": "lili", "password": "lola"})
    sock.send(msg.encode())
    response = sock.recv(4096).decode()
    print(f"Login: {response}")
    time.sleep(0.5)
    
    # Create session
    msg = "POST session/create\n" + json.dumps({
        "name": "Test",
        "themeIds": [0],
        "difficulty": "facile",
        "nbQuestions": 5,
        "timeLimit": 30,
        "mode": "solo",
        "maxPlayers": 1
    })
    sock.send(msg.encode())
    response = sock.recv(4096).decode()
    print(f"Create: {response}")
    time.sleep(0.5)
    
    # Start session
    print("Starting session...")
    msg = "POST session/start\n"
    sock.send(msg.encode())
    
    # Recevoir les messages
    print("Waiting for messages...")
    for i in range(5):
        response = sock.recv(4096).decode()
        print(f"Message {i+1}: {response}")
        time.sleep(1)
    
    sock.close()

if __name__ == "__main__":
    test_session()
