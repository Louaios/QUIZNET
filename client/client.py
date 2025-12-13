import socket
import json
import threading
import time

class QuizNetClient:
    """
    Classe gérant la communication réseau avec le serveur QuizNet
    """
    
    def __init__(self):
        self.tcp_socket = None
        self.server_ip = None
        self.server_port = None
        
        self.username = None
        self.session_id = None
        
        self.connected = False
        self.running = False
        
        self.callback = None
        self.receive_thread = None
    
    # ========================================================================
    # DÉCOUVERTE DES SERVEURS (UDP Broadcast)
    # ========================================================================
    def discover_servers(self, timeout=3):
        """
        Découvre les serveurs QuizNet sur le réseau local via UDP broadcast
        """
        servers = []
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.settimeout(timeout)
        
        message = "looking for quiznet servers"
        
        try:
            sock.sendto(message.encode(), ('<broadcast>', 5555))
            
            start_time = time.time()
            while time.time() - start_time < timeout:
                try:
                    data, addr = sock.recvfrom(1024)
                    response = data.decode().strip()
                    
                    if response.startswith("hello i'm a quiznet server:"):
                        parts = response.split(":")
                        if len(parts) >= 3:
                            server_name = parts[1]
                            server_port = int(parts[2])
                            servers.append({
                                'name': server_name,
                                'ip': addr[0],
                                'port': server_port
                            })
                except socket.timeout:
                    break
        except Exception as e:
            print(f"Erreur découverte serveurs: {e}")
        finally:
            sock.close()
            
        return servers
    
    # ========================================================================
    # CONNEXION AU SERVEUR (TCP)
    # ========================================================================
    def connect_to_server(self, ip, port):
        """
        Se connecte à un serveur QuizNet via TCP
        """
        try:
            self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.tcp_socket.connect((ip, port))
            self.server_ip = ip
            self.server_port = port
            self.connected = True
            
            self.running = True
            self.receive_thread = threading.Thread(target=self._receive_loop, daemon=True)
            self.receive_thread.start()
            
            return True
        except Exception as e:
            print(f"Erreur connexion: {e}")
            return False
    
    # ========================================================================
    # BOUCLE DE RÉCEPTION DES MESSAGES
    # ========================================================================
    def _receive_loop(self):
        """
        Thread de réception des messages du serveur
        """
        buffer = ""
        while self.running:
            try:
                data = self.tcp_socket.recv(4096)
                if not data:
                    print("Connexion fermée par le serveur")
                    break
                    
                buffer += data.decode('utf-8', errors='ignore')
                
                while '\n' in buffer:
                    line_end = buffer.index('\n')
                    line = buffer[:line_end].strip()
                    buffer = buffer[line_end + 1:]
                    
                    if not line:
                        continue
                    
                    try:
                        message = json.loads(line)
                        print(f"Message JSON reçu: {message}")
                        
                        if self.callback:
                            self.callback(message)
                    except json.JSONDecodeError as e:
                        print(f"Erreur JSON: {e}")
                        print(f"Ligne problématique: {line}")
                        
            except Exception as e:
                print(f"Erreur réception: {e}")
                break
        
        self.connected = False
        print("Thread de réception terminé")
    
    # ========================================================================
    # ENVOI D'UNE REQUÊTE AU SERVEUR
    # ========================================================================
    def send_request(self, method, action, data=None):
        """
        Envoie une requête au serveur
        Format: "METHOD action\nJSON"
        """
        if not self.connected:
            return False
            
        try:
            message = f"{method} {action}"
            if data:
                message += "\n" + json.dumps(data, ensure_ascii=False)
            else:
                message += "\n"
            
            self.tcp_socket.send(message.encode('utf-8'))
            return True
        except Exception as e:
            print(f"Erreur envoi: {e}")
            return False
    
    # ========================================================================
    # MÉTHODES DE COMMUNICATION AVEC LE SERVEUR
    # ========================================================================
    
    def register(self, pseudo, password):
        """Inscrit un nouveau joueur"""
        return self.send_request("POST", "player/register", {
            "pseudo": pseudo,
            "password": password
        })
    
    def login(self, pseudo, password):
        """Connecte un joueur"""
        self.username = pseudo
        return self.send_request("POST", "player/login", {
            "pseudo": pseudo,
            "password": password
        })
    
    def get_sessions_list(self):
        """Récupère la liste des sessions disponibles"""
        return self.send_request("GET", "sessions/list")
    
    def create_session(self, name, theme_ids, difficulty, nb_questions, time_limit, mode, max_players):
        """Crée une nouvelle session"""
        return self.send_request("POST", "session/create", {
            "name": name,
            "themeIds": theme_ids,
            "difficulty": difficulty,
            "nbQuestions": nb_questions,
            "timeLimit": time_limit,
            "mode": mode,
            "maxPlayers": max_players
        })
    
    def join_session(self, session_id):
        """Rejoint une session existante"""
        self.session_id = session_id
        return self.send_request("POST", "session/join", {
            "sessionId": session_id
        })
    
    def start_session(self):
        """Démarre une session (créateur uniquement)"""
        return self.send_request("POST", "session/start")
    
    def use_joker(self, joker_type):
        """Utilise un joker"""
        return self.send_request("POST", "joker/use", {
            "type": joker_type
        })

    def send_chat(self, message):
        """Envoie un message de chat à la session courante"""
        if not self.session_id:
            return False
        return self.send_request("POST", "chat/send", {"message": message})

    def get_chat_history(self, session_id=None):
        """Demande l'historique de chat pour la session (utilise la session courante si None)"""
        sid = session_id if session_id is not None else self.session_id
        if not sid:
            return False
        return self.send_request("GET", "chat/history", {"sessionId": sid})
    
    def answer_question(self, answer, response_time):
        """Envoie une réponse à une question"""
        return self.send_request("POST", "question/answer", {
            "answer": answer,
            "responseTime": response_time
        })
    
    # ========================================================================
    # UTILITAIRES
    # ========================================================================
    
    def set_callback(self, callback):
        """Définit la fonction callback pour les messages reçus"""
        self.callback = callback
    
    def disconnect(self):
        """Ferme la connexion"""
        self.running = False
        if self.tcp_socket:
            try:
                self.tcp_socket.close()
            except:
                pass
        self.connected = False
