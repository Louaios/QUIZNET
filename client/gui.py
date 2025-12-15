import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time
from client import QuizNetClient

class QuizNetGUI:
    """
    Interface graphique principale du client QuizNet
    Toutes les corrections intégrées
    """
    
    def __init__(self):
        self.client = QuizNetClient()
        self.client.set_callback(self.handle_server_message)
        
        self.root = tk.Tk()
        self.root.title("QuizNet Client")
        self.root.geometry("800x600")
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        
        self.current_screen = None
        self.is_creator = False
        self.current_question = None
        self.question_start_time = None
        self.jokers = {"fifty": 1, "skip": 1}
        self.lives = 4
        self.current_mode = "solo"
        self.time_limit = 30
        self.available_sessions = []
        
        self.container = tk.Frame(self.root)
        self.container.pack(fill="both", expand=True)
        
        self.show_discovery_screen()
    
    def clear_screen(self):
        for widget in self.container.winfo_children():
            widget.destroy()

    def open_chat_window(self):
        if hasattr(self, 'chat_window') and self.chat_window.winfo_exists():
            try:
                self.chat_window.deiconify()
                self.chat_window.lift()
            except Exception:
                pass
            return

        self.chat_window = tk.Toplevel(self.root)
        self.chat_window.title("Chat - Session")
        self.chat_window.geometry("360x420")

        chat_frame = tk.Frame(self.chat_window)
        chat_frame.pack(fill="both", expand=True, padx=8, pady=8)

        self.chat_display = tk.Text(chat_frame, state='disabled', wrap='word', height=18)
        self.chat_display.pack(side='left', fill='both', expand=True)

        scrollbar = ttk.Scrollbar(chat_frame, command=self.chat_display.yview)
        scrollbar.pack(side='right', fill='y')
        self.chat_display['yscrollcommand'] = scrollbar.set

        entry_frame = tk.Frame(self.chat_window)
        entry_frame.pack(fill='x', padx=8, pady=(0,8))

        self.chat_entry = tk.Entry(entry_frame, font=("Arial", 11))
        self.chat_entry.pack(side='left', fill='x', expand=True, padx=(0,8))
        self.chat_entry.bind('<Return>', lambda e: self._chat_send())

        send_btn = tk.Button(entry_frame, text="Envoyer", command=self._chat_send,
                             bg="#2196F3", fg="white", font=("Arial", 11), width=10)
        send_btn.pack(side='right')

        self.chat_window.protocol("WM_DELETE_WINDOW", lambda: self.chat_window.withdraw())

    def append_chat_message(self, pseudo, message, timestamp=None):
        if not hasattr(self, 'chat_display'):
            return
        if not timestamp:
            timestamp = time.strftime('%Y-%m-%d %H:%M:%S')

        try:
            self.chat_display.config(state='normal')
            self.chat_display.insert(tk.END, f"[{timestamp}] {pseudo}: {message}\n")
            self.chat_display.see(tk.END)
            self.chat_display.config(state='disabled')
        except tk.TclError:
            pass

    def _chat_send(self):
        if not hasattr(self, 'chat_entry'): return
        text = self.chat_entry.get().strip()
        if not text: return
        self.chat_entry.delete(0, tk.END)

        def _call():
            self.client.send_chat(text)

        threading.Thread(target=_call, daemon=True).start()
    
    def show_discovery_screen(self):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True)
        
        tk.Label(frame, text="QuizNet", font=("Arial", 32, "bold")).pack(pady=20)
        tk.Label(frame, text="Recherche de serveurs...", font=("Arial", 14)).pack(pady=10)
        
        progress = ttk.Progressbar(frame, mode='indeterminate', length=300)
        progress.pack(pady=20)
        progress.start()
        
        def discover():
            servers = self.client.discover_servers(timeout=1)
            self.root.after(0, lambda: self.show_server_list(servers))
        
        threading.Thread(target=discover, daemon=True).start()
    
    def show_server_list(self, servers):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True)
        
        tk.Label(frame, text="Serveurs disponibles", font=("Arial", 20, "bold")).pack(pady=20)
        
        if not servers:
            tk.Label(frame, text="Aucun serveur trouvé", fg="red").pack(pady=10)
            
            manual_frame = tk.Frame(frame)
            manual_frame.pack(pady=20)
            
            tk.Label(manual_frame, text="Connexion manuelle:", font=("Arial", 12, "bold")).pack()
            tk.Label(manual_frame, text="Adresse IP du serveur:").pack()
            ip_entry = tk.Entry(manual_frame, width=20, font=("Arial", 11))
            ip_entry.pack(pady=5)
            ip_entry.insert(0, "127.0.0.1")
            
            def connect_manual():
                ip = ip_entry.get().strip()
                if ip and self.client.connect_to_server(ip, 5556):
                    self.show_login_screen()
                else:
                    messagebox.showerror("Erreur", "Impossible de se connecter au serveur")
            
            tk.Button(manual_frame, text="Se connecter", command=connect_manual,
                     bg="#FF9800", fg="white", font=("Arial", 12), width=20).pack(pady=10)
            
            tk.Button(frame, text="Réessayer la découverte", command=self.show_discovery_screen,
                     bg="#2196F3", fg="white", font=("Arial", 12), width=20).pack(pady=10)
            return
        
        listbox = tk.Listbox(frame, width=50, height=10, font=("Arial", 12))
        listbox.pack(pady=10)
        
        for server in servers:
            listbox.insert(tk.END, f"{server['name']} ({server['ip']}:{server['port']})")
        
        def connect():
            selection = listbox.curselection()
            if not selection:
                messagebox.showwarning("Sélection", "Veuillez sélectionner un serveur")
                return
            
            server = servers[selection[0]]
            if self.client.connect_to_server(server['ip'], server['port']):
                self.show_login_screen()
            else:
                messagebox.showerror("Erreur", "Impossible de se connecter au serveur")
        
        tk.Button(frame, text="Se connecter", command=connect, 
                 bg="#4CAF50", fg="white", font=("Arial", 12), width=20).pack(pady=10)
    
    def show_login_screen(self):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True)
        
        tk.Label(frame, text="Connexion", font=("Arial", 24, "bold")).pack(pady=20)
        
        form_frame = tk.Frame(frame)
        form_frame.pack(pady=20)
        
        tk.Label(form_frame, text="Pseudo:", font=("Arial", 12)).grid(row=0, column=0, padx=10, pady=10, sticky="e")
        pseudo_entry = tk.Entry(form_frame, font=("Arial", 12), width=25)
        pseudo_entry.grid(row=0, column=1, padx=10, pady=10)
        
        tk.Label(form_frame, text="Mot de passe:", font=("Arial", 12)).grid(row=1, column=0, padx=10, pady=10, sticky="e")
        password_entry = tk.Entry(form_frame, font=("Arial", 12), width=25, show="*")
        password_entry.grid(row=1, column=1, padx=10, pady=10)
        
        btn_frame = tk.Frame(frame)
        btn_frame.pack(pady=20)
        
        def login():
            pseudo = pseudo_entry.get().strip()
            password = password_entry.get().strip()
            
            if not pseudo or not password:
                messagebox.showwarning("Erreur", "Veuillez remplir tous les champs")
                return
            
            self.client.login(pseudo, password)
        
        def register():
            pseudo = pseudo_entry.get().strip()
            password = password_entry.get().strip()
            
            if not pseudo or not password:
                messagebox.showwarning("Erreur", "Veuillez remplir tous les champs")
                return
            
            self.client.register(pseudo, password)
        
        tk.Button(btn_frame, text="Se connecter", command=login,
                 bg="#2196F3", fg="white", font=("Arial", 12), width=15).pack(side="left", padx=10)
        tk.Button(btn_frame, text="S'inscrire", command=register,
                 bg="#4CAF50", fg="white", font=("Arial", 12), width=15).pack(side="left", padx=10)
    
    def show_main_menu(self):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True)
        
        tk.Label(frame, text=f"Bienvenue {self.client.username}!", 
                font=("Arial", 24, "bold")).pack(pady=20)
        
        btn_width = 20
        tk.Button(frame, text="Créer une session", command=self.show_create_session,
                 bg="#4CAF50", fg="white", font=("Arial", 14), width=btn_width).pack(pady=10)
        tk.Button(frame, text="Rejoindre une session", command=self.show_sessions_list,
                 bg="#2196F3", fg="white", font=("Arial", 14), width=btn_width).pack(pady=10)
        tk.Button(frame, text="Quitter", command=self.on_closing,
                 bg="#f44336", fg="white", font=("Arial", 14), width=btn_width).pack(pady=10)
        if hasattr(self, 'chat_window'):
            try:
                self.chat_window.destroy()
            except Exception:
                pass
            delattr(self, 'chat_window')
    
    def show_create_session(self):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True, fill="both")
        
        tk.Label(frame, text="Créer une session", font=("Arial", 20, "bold")).pack(pady=20)
        
        form_frame = tk.Frame(frame)
        form_frame.pack(pady=10)
        
        tk.Label(form_frame, text="Nom de la session:", font=("Arial", 11)).grid(row=0, column=0, padx=10, pady=5, sticky="e")
        name_entry = tk.Entry(form_frame, font=("Arial", 11), width=25)
        name_entry.grid(row=0, column=1, padx=10, pady=5)
        name_entry.insert(0, f"Session de {self.client.username}")
        
        tk.Label(form_frame, text="Thème:", font=("Arial", 11)).grid(row=1, column=0, padx=10, pady=5, sticky="e")
        theme_var = tk.StringVar(value="Tous")
        theme_combo = ttk.Combobox(form_frame, textvariable=theme_var, font=("Arial", 11), width=23,
                                   values=["Tous", "Informatique", "Histoire", "Science", "Mathématiques", "Culture Générale"])
        theme_combo.grid(row=1, column=1, padx=10, pady=5)
        
        tk.Label(form_frame, text="Difficulté:", font=("Arial", 11)).grid(row=2, column=0, padx=10, pady=5, sticky="e")
        difficulty_var = tk.StringVar(value="moyen")
        difficulty_combo = ttk.Combobox(form_frame, textvariable=difficulty_var, font=("Arial", 11), width=23,
                                       values=["facile", "moyen", "difficile"])
        difficulty_combo.grid(row=2, column=1, padx=10, pady=5)
        
        tk.Label(form_frame, text="Nombre de questions:", font=("Arial", 11)).grid(row=3, column=0, padx=10, pady=5, sticky="e")
        nb_questions_var = tk.IntVar(value=5)
        nb_questions_spin = tk.Spinbox(form_frame, from_=5, to=30, textvariable=nb_questions_var, 
                                      font=("Arial", 11), width=23)
        nb_questions_spin.grid(row=3, column=1, padx=10, pady=5)
        
        tk.Label(form_frame, text="Temps limite (sec):", font=("Arial", 11)).grid(row=4, column=0, padx=10, pady=5, sticky="e")
        time_limit_var = tk.IntVar(value=30)
        time_limit_spin = tk.Spinbox(form_frame, from_=10, to=60, textvariable=time_limit_var,
                                     font=("Arial", 11), width=23)
        time_limit_spin.grid(row=4, column=1, padx=10, pady=5)
        
        tk.Label(form_frame, text="Mode:", font=("Arial", 11)).grid(row=5, column=0, padx=10, pady=5, sticky="e")
        mode_var = tk.StringVar(value="solo")
        mode_combo = ttk.Combobox(form_frame, textvariable=mode_var, font=("Arial", 11), width=23,
                                 values=["solo", "battle"])
        mode_combo.grid(row=5, column=1, padx=10, pady=5)
        
        tk.Label(form_frame, text="Nombre de joueurs:", font=("Arial", 11)).grid(row=6, column=0, padx=10, pady=5, sticky="e")
        max_players_var = tk.IntVar(value=1)
        max_players_spin = tk.Spinbox(form_frame, from_=1, to=4, textvariable=max_players_var,
                                     font=("Arial", 11), width=23, state='disabled')
        max_players_spin.grid(row=6, column=1, padx=10, pady=5)
        
        def on_mode_change(event):
            if mode_var.get() == "battle":
                max_players_spin.config(state='normal')
                max_players_var.set(2)
            else:
                max_players_spin.config(state='disabled')
                max_players_var.set(1)
        
        mode_combo.bind('<<ComboboxSelected>>', on_mode_change)
        
        btn_frame = tk.Frame(frame)
        btn_frame.pack(pady=20)
        
        def create():
            name = name_entry.get().strip()
            if not name:
                messagebox.showwarning("Erreur", "Veuillez entrer un nom de session")
                return
            
            self.current_mode = mode_var.get()
            self.time_limit = time_limit_var.get()
            
            self.client.create_session(
                name=name,
                theme_ids=[0],
                difficulty=difficulty_var.get(),
                nb_questions=nb_questions_var.get(),
                time_limit=time_limit_var.get(),
                mode=mode_var.get(),
                max_players=max_players_var.get()
            )
        
        tk.Button(btn_frame, text="Créer", command=create,
                 bg="#4CAF50", fg="white", font=("Arial", 12), width=15).pack(side="left", padx=10)
        tk.Button(btn_frame, text="Retour", command=self.show_main_menu,
                 bg="#757575", fg="white", font=("Arial", 12), width=15).pack(side="left", padx=10)
    
    def show_sessions_list(self):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True, fill="both")
        
        tk.Label(frame, text="Sessions disponibles", font=("Arial", 20, "bold")).pack(pady=20)
        
        columns = ("Nom", "Mode", "Difficulté", "Joueurs")
        tree = ttk.Treeview(frame, columns=columns, show="headings", height=10)
        
        for col in columns:
            tree.heading(col, text=col)
            tree.column(col, width=180)
        
        tree.pack(pady=10, fill="both", expand=True, padx=20)
        
        self.sessions_tree = tree
        
        btn_frame = tk.Frame(frame)
        btn_frame.pack(pady=10)
        
        def join():
            selection = tree.selection()
            if not selection:
                messagebox.showwarning("Sélection", "Veuillez sélectionner une session")
                return
            
            item = tree.item(selection[0])
            session_name = item['values'][0]
            
            for session in self.available_sessions:
                if session.get('name') == session_name:
                    print(f"Joining session {session['id']}")
                    self.client.join_session(session['id'])
                    return
        
        def refresh():
            self.client.get_sessions_list()
        
        tk.Button(btn_frame, text="Rejoindre", command=join,
                 bg="#4CAF50", fg="white", font=("Arial", 12), width=15).pack(side="left", padx=10)
        tk.Button(btn_frame, text="Actualiser", command=refresh,
                 bg="#2196F3", fg="white", font=("Arial", 12), width=15).pack(side="left", padx=10)
        tk.Button(btn_frame, text="Retour", command=self.show_main_menu,
                 bg="#757575", fg="white", font=("Arial", 12), width=15).pack(side="left", padx=10)
        
        self.client.get_sessions_list()
    
    def update_sessions_list(self, sessions):
        if not hasattr(self, 'sessions_tree'):
            return
        
        print(f"Updating sessions list: {len(sessions)} sessions")
        
        self.available_sessions = sessions
        
        for item in self.sessions_tree.get_children():
            self.sessions_tree.delete(item)
        
        for session in sessions:
            self.sessions_tree.insert('', 'end', values=(
                session.get('name', 'N/A'),
                session.get('mode', 'N/A'),
                session.get('difficulty', 'N/A'),
                f"{session.get('nbPlayers', 0)}/{session.get('maxPlayers', 1)}"
            ))
    
    def show_waiting_room(self, session_data):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True)
        
        tk.Label(frame, text="Salle d'attente", font=("Arial", 24, "bold")).pack(pady=20)
        
        info_frame = tk.Frame(frame, bg="#f0f0f0", padx=20, pady=20)
        info_frame.pack(pady=10, fill="x", padx=50)
        
        tk.Label(info_frame, text=f"Mode: {session_data.get('mode', 'N/A').upper()}", 
                font=("Arial", 12), bg="#f0f0f0").pack(anchor="w")
        
        tk.Label(frame, text="Joueurs connectés:", font=("Arial", 14, "bold")).pack(pady=(20, 10))
        
        players_listbox = tk.Listbox(frame, font=("Arial", 12), width=40, height=6)
        players_listbox.pack(pady=10)
        self.players_listbox = players_listbox
        
        if 'players' in session_data:
            for player in session_data['players']:
                players_listbox.insert(tk.END, f"🎮 {player}")
        
        if session_data.get('isCreator'):
            self.is_creator = True
            self.start_button = tk.Button(frame, text="Démarrer la partie", 
                                          command=self.start_game,
                                          bg="#4CAF50", fg="white", 
                                          font=("Arial", 14), width=20)
            self.start_button.pack(pady=20)
        else:
            tk.Label(frame, text="En attente du créateur...", 
                    font=("Arial", 12), fg="gray").pack(pady=20)

        try:
            self.open_chat_window()
        except Exception:
            pass
    
    def start_game(self):
        print("Démarrage de la partie...")
        
        if hasattr(self, 'start_button'):
            self.start_button.config(state='disabled', text="Démarrage en cours...")
        
        self.client.start_session()
    
    def show_countdown(self, count):
        print(f"Countdown: {count}")
        self.clear_screen()
        
        if hasattr(self, 'countdown_timer'):
            self.root.after_cancel(self.countdown_timer)
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True)
        
        label = tk.Label(frame, text=str(count) if count > 0 else "GO!", 
                        font=("Arial", 100, "bold"), fg="#4CAF50")
        label.pack(expand=True)
        
        if count > 0:
            self.countdown_timer = self.root.after(1000, lambda: self.show_countdown(count - 1))

    def show_question(self, question_data):
        print(f"Affichage question: {question_data.get('question', 'N/A')}")
        
        if hasattr(self, 'countdown_timer'):
            self.root.after_cancel(self.countdown_timer)
            delattr(self, 'countdown_timer')
        
        if hasattr(self, 'question_timer'):
            self.root.after_cancel(self.question_timer)
        
        self.clear_screen()
        self.current_question = question_data
        self.question_start_time = time.time()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True, fill="both", padx=20, pady=20)
        
        header_frame = tk.Frame(frame)
        header_frame.pack(fill="x", pady=(0, 20))
        
        tk.Label(header_frame, 
                text=f"Question {question_data.get('questionNum', '?')}/{question_data.get('totalQuestions', '?')}",
                font=("Arial", 14, "bold")).pack(side="left")
        
        if self.current_mode == "battle":
            lives_frame = tk.Frame(header_frame, bg="#ffebee", padx=10, pady=5)
            lives_frame.pack(side="left", padx=20)
            
            hearts = "❤️" * self.lives + "🖤" * (4 - self.lives)
            tk.Label(lives_frame, text=f"Vies: {hearts}", 
                    font=("Arial", 12), bg="#ffebee").pack()
        
        self.timer_label = tk.Label(header_frame, text=f"⏱️ {self.time_limit}s", 
                                    font=("Arial", 14, "bold"), fg="#f44336")
        self.timer_label.pack(side="right")
        self.update_timer()
        
        question_frame = tk.Frame(frame, bg="#e3f2fd", padx=20, pady=20)
        question_frame.pack(fill="x", pady=20)
        
        # question de type text fixed #
        # s'assurer que la question n'est pas None ou vide #
        qtext = question_data.get('question', '')
        if qtext is None:
            qtext = ''
        qtext = str(qtext).strip()
        if not qtext:
            qtext = "[Question vide]"

        tk.Message(question_frame, text=qtext,
                   font=("Arial", 16, "bold"), bg="#e3f2fd",
                   width=700, anchor='w', justify='left').pack(fill='x')
        
        if self.jokers.get("fifty", 0) > 0 or self.jokers.get("skip", 0) > 0:
            jokers_frame = tk.Frame(frame, bg="#FFF3E0", padx=10, pady=10)
            jokers_frame.pack(fill="x", pady=10)
            
            tk.Label(jokers_frame, text="🃏 Jokers disponibles:", 
                    font=("Arial", 12, "bold"), bg="#FFF3E0").pack(side="left", padx=10)
            
            if self.jokers.get("fifty", 0) > 0:
                def use_fifty():
                    def _call():
                        self.client.use_joker("fifty")
                    threading.Thread(target=_call, daemon=True).start()
                    self.jokers["fifty"] = 0
                
                tk.Button(jokers_frame, text="50/50", command=use_fifty,
                        bg="#FF9800", fg="white", font=("Arial", 11), width=10).pack(side="left", padx=5)
            
            if self.jokers.get("skip", 0) > 0:
                def use_skip():
                    print("Utilisation du joker skip...")
                    
                    if hasattr(self, 'question_timer'):
                        self.root.after_cancel(self.question_timer)
                        delattr(self, 'question_timer')
                    
                    self.question_start_time = None
                    
                    self.show_waiting_results()

                    def _call():
                        self.client.use_joker("skip")
                    threading.Thread(target=_call, daemon=True).start()
                    self.jokers["skip"] = 0
                
                tk.Button(jokers_frame, text="⏭️ Passer", command=use_skip,
                        bg="#9C27B0", fg="white", font=("Arial", 11), width=10).pack(side="left", padx=5)
        
        answers_frame = tk.Frame(frame)
        answers_frame.pack(expand=True, fill="both", pady=20)
        
        q_type = question_data.get('type', 'qcm')
        
        if q_type == 'qcm':
            self.show_qcm_answers(answers_frame, question_data.get('answers', []))
        elif q_type == 'boolean':
            self.show_boolean_answers(answers_frame)
        elif q_type == 'text':
            self.show_text_answer(answers_frame)

    def show_qcm_answers(self, parent, answers):
        """🔧 CORRECTION : stocker les boutons pour le 50/50"""
        self.answer_var = tk.IntVar(value=-1)
        self.qcm_buttons = []  # Pour le joker 50/50
        
        for i, answer in enumerate(answers):
            btn = tk.Radiobutton(parent, text=answer, variable=self.answer_var, value=i,
                                font=("Arial", 12), anchor="w", padx=20, pady=10,
                                bg="white", selectcolor="#4CAF50")
            btn.pack(fill="x", pady=5)
            self.qcm_buttons.append(btn)
        
        tk.Button(parent, text="Valider", command=self.submit_answer,
                 bg="#4CAF50", fg="white", font=("Arial", 14), width=20).pack(pady=20)
    
    def show_boolean_answers(self, parent):
        self.answer_var = tk.StringVar(value="")

        btn_frame = tk.Frame(parent)
        btn_frame.pack(expand=True)

        # creer les boutons radio pour les réponses booléennes #
        # s'assurer  que seul le bouton sélectionné reste actif visuellement.  #
        self.boolean_true_btn = tk.Radiobutton(
            btn_frame, text="✓ VRAI", variable=self.answer_var, value="true",
            font=("Arial", 16, "bold"), fg="#4CAF50",
            indicatoron=1, padx=30, pady=20,
            command=lambda: self._on_boolean_selected("true")
        )
        self.boolean_true_btn.pack(side="left", padx=20)

        self.boolean_false_btn = tk.Radiobutton(
            btn_frame, text="✗ FAUX", variable=self.answer_var, value="false",
            font=("Arial", 16, "bold"), fg="#f44336",
            indicatoron=1, padx=30, pady=20,
            command=lambda: self._on_boolean_selected("false")
        )
        self.boolean_false_btn.pack(side="left", padx=20)
        
        tk.Button(parent, text="Valider", command=self.submit_answer,
                 bg="#4CAF50", fg="white", font=("Arial", 14), width=20).pack(pady=40)
    
    def show_text_answer(self, parent):
        tk.Label(parent, text="Votre réponse:", font=("Arial", 12)).pack(pady=10)
        
        self.text_answer_entry = tk.Entry(parent, font=("Arial", 14), width=40)
        self.text_answer_entry.pack(pady=10)
        self.text_answer_entry.focus()
        self.text_answer_entry.bind("<Return>", lambda e: self.submit_answer())
        
        tk.Button(parent, text="Valider", command=self.submit_answer,
                 bg="#4CAF50", fg="white", font=("Arial", 14), width=20).pack(pady=20)

    def _on_boolean_selected(self, val):
        # s'assurer que seul le bouton sélectionné reste actif visuellement.  #
        try:
            self.answer_var.set(val)
            if hasattr(self, 'boolean_true_btn') and hasattr(self, 'boolean_false_btn'):
                if val == 'true':
                    self.boolean_true_btn.select()
                    self.boolean_false_btn.deselect()
                else:
                    self.boolean_false_btn.select()
                    self.boolean_true_btn.deselect()
        except Exception:
            pass
    
    def update_timer(self):
        if not hasattr(self, 'timer_label'):
            return
        
        if not self.question_start_time:
            return
        
        try:
            elapsed = time.time() - self.question_start_time
            remaining = max(0, self.time_limit - int(elapsed))
            
            self.timer_label.config(text=f"⏱️ {remaining}s")
            
            if remaining > 0:
                self.question_timer = self.root.after(1000, self.update_timer)
            else:
                self.submit_answer(timeout=True)
        except tk.TclError:
            pass
    
    def submit_answer(self, timeout=False):
        if not self.question_start_time:
            return
        
        if hasattr(self, 'question_timer'):
            self.root.after_cancel(self.question_timer)
            delattr(self, 'question_timer')
        
        response_time = time.time() - self.question_start_time
        
        if timeout:
            answer = -1
        else:
            q_type = self.current_question.get('type', 'qcm')
            
            if q_type == 'qcm':
                answer = self.answer_var.get()
                if answer == -1:
                    messagebox.showwarning("Attention", "Veuillez sélectionner une réponse")
                    self.update_timer()
                    return
            elif q_type == 'boolean':
                answer = self.answer_var.get()
                if not answer:
                    messagebox.showwarning("Attention", "Veuillez sélectionner une réponse")
                    self.update_timer()
                    return
            elif q_type == 'text':
                answer = self.text_answer_entry.get().strip()
                if not answer:
                    messagebox.showwarning("Attention", "Veuillez entrer une réponse")
                    self.update_timer()
                    return
        
        self.question_start_time = None
        self.client.answer_question(answer, response_time)
        self.show_waiting_results()
    
    def show_waiting_results(self):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True)
        
        tk.Label(frame, text="Réponse envoyée ✓", font=("Arial", 24, "bold"), fg="#4CAF50").pack(pady=20)
        tk.Label(frame, text="En attente des résultats...", font=("Arial", 14)).pack(pady=10)
    
    def show_question_results(self, results_data):
        """🔧 CORRECTION : Affichage résultats intermédiaires"""
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True, fill="both", padx=20, pady=20)
        
        correct_idx = results_data.get('correctAnswer', -1)
        
        if self.current_question:
            q_type = self.current_question.get('type', 'qcm')
            
            if q_type == 'qcm':
                answers = self.current_question.get('answers', [])
                if 0 <= correct_idx < len(answers):
                    correct_text = answers[correct_idx]
                else:
                    correct_text = "Erreur"
            elif q_type == 'boolean':
                correct_text = "VRAI" if correct_idx == 0 else "FAUX"
            else:
                correct_text = "Voir correction"
            
            tk.Label(frame, text="✅ Bonne réponse :", 
                    font=("Arial", 16, "bold")).pack(pady=10)
            tk.Label(frame, text=correct_text, 
                    font=("Arial", 20), fg="#4CAF50", wraplength=600).pack(pady=10)
        
        tk.Label(frame, text="📊 Classement actuel", 
                font=("Arial", 16, "bold")).pack(pady=(20, 10))
        
        ranking_frame = tk.Frame(frame, bg="white", relief="solid", borderwidth=1)
        ranking_frame.pack(pady=10, fill="both", expand=True)
        
        for player in results_data.get('ranking', []):
            rank = player.get('rank', '?')
            pseudo = player.get('pseudo', 'Unknown')
            score = player.get('score', 0)
            
            medal = {1: "🥇", 2: "🥈", 3: "🥉"}.get(rank, f"{rank}.")
            
            player_frame = tk.Frame(ranking_frame, bg="white")
            player_frame.pack(fill="x", pady=5, padx=10)
            
            tk.Label(player_frame, text=f"{medal} {pseudo}", 
                    font=("Arial", 12), bg="white", width=25, anchor="w").pack(side="left", padx=10)
            tk.Label(player_frame, text=f"{score} pts", 
                    font=("Arial", 12, "bold"), fg="#2196F3", bg="white").pack(side="left")
            
            if self.current_mode == "battle" and 'lives' in player:
                lives = player.get('lives', 0)
                hearts = "❤️" * lives + "🖤" * (4 - lives)
                tk.Label(player_frame, text=hearts, 
                        font=("Arial", 10), bg="white").pack(side="right", padx=10)
        
        tk.Label(frame, text="Question suivante dans 3 secondes...", 
                font=("Arial", 11), fg="gray").pack(pady=10)
    
    def show_final_results(self, final_data):
        self.clear_screen()
        
        frame = tk.Frame(self.container)
        frame.pack(expand=True, fill="both", padx=20, pady=20)
        
        tk.Label(frame, text="🏆 Partie terminée !", 
                font=("Arial", 28, "bold"), fg="#FFD700").pack(pady=20)
        
        tk.Label(frame, text="Classement final", font=("Arial", 18, "bold")).pack(pady=(20, 10))
        
        ranking_frame = tk.Frame(frame, bg="white", relief="solid", borderwidth=1)
        ranking_frame.pack(fill="both", expand=True, pady=10)
        
        headers = ["Rang", "Joueur", "Score"]
        if self.current_mode == "battle":
            headers.append("Vies")
        
        header_frame = tk.Frame(ranking_frame, bg="#e0e0e0")
        header_frame.pack(fill="x")
        
        for i, header in enumerate(headers):
            tk.Label(header_frame, text=header, font=("Arial", 12, "bold"), 
                    bg="#e0e0e0", padx=15, pady=8).grid(row=0, column=i, sticky="ew")
        
        for idx, player in enumerate(final_data.get('ranking', []), start=1):
            rank = player.get('rank', idx)
            pseudo = player.get('pseudo', 'N/A')
            score = player.get('score', 0)
            
            if rank == 1:
                bg_color = "#FFD700"
                medal = "🥇"
            elif rank == 2:
                bg_color = "#C0C0C0"
                medal = "🥈"
            elif rank == 3:
                bg_color = "#CD7F32"
                medal = "🥉"
            else:
                bg_color = "white"
                medal = ""
            
            row_frame = tk.Frame(ranking_frame, bg=bg_color)
            row_frame.pack(fill="x")
            
            tk.Label(row_frame, text=f"{medal} {rank}", bg=bg_color, 
                    padx=15, pady=8, font=("Arial", 11, "bold")).grid(row=0, column=0, sticky="ew")
            tk.Label(row_frame, text=pseudo, bg=bg_color, padx=15, pady=8).grid(row=0, column=1, sticky="ew")
            tk.Label(row_frame, text=f"{score} pts", bg=bg_color, padx=15, pady=8).grid(row=0, column=2, sticky="ew")
            
            if self.current_mode == "battle":
                lives = player.get('lives', 0)
                eliminated = player.get('eliminated', False)
                if eliminated:
                    lives_text = "❌ Éliminé"
                else:
                    lives_text = "❤️" * lives
                tk.Label(row_frame, text=lives_text, bg=bg_color, padx=15, pady=8).grid(row=0, column=3, sticky="ew")
        
        tk.Button(frame, text="Retour au menu", command=self.show_main_menu,
                 bg="#2196F3", fg="white", font=("Arial", 14), width=20).pack(pady=30)
    
    def handle_server_message(self, message):
        """🔧 CORRECTION : Gestion complète des messages serveur"""
        action = message.get('action', '')
        statut = message.get('statut', '')
        
        print(f"Message reçu: action={action}, statut={statut}")
        
        if action == 'player/register':
            if statut == '201':
                self.root.after(0, lambda: messagebox.showinfo("Succès", "Inscription réussie !"))
            else:
                msg = message.get('message', 'Erreur inconnue')
                self.root.after(0, lambda m=msg: messagebox.showerror("Erreur", m))
        
        elif action == 'player/login':
            if statut == '200':
                self.root.after(0, self.show_main_menu)
            else:
                msg = message.get('message', 'Erreur inconnue')
                self.root.after(0, lambda m=msg: messagebox.showerror("Erreur", m))
        
        elif action == 'sessions/list':
            if statut == '200':
                sessions = message.get('sessions', [])
                self.root.after(0, lambda s=sessions: self.update_sessions_list(s))
        
        elif action == 'session/create':
            if statut == '201':
                self.is_creator = True
                self.jokers = message.get('jokers', {"fifty": 1, "skip": 1})
                if 'lives' in message:
                    self.lives = message['lives']
                self.root.after(0, lambda m=message: self.show_waiting_room(m))
            else:
                msg = message.get('message', 'Erreur inconnue')
                self.root.after(0, lambda m=msg: messagebox.showerror("Erreur", m))
        
        elif action == 'session/join':
            if statut == '200':
                self.is_creator = False
                self.jokers = message.get('jokers', {"fifty": 1, "skip": 1})
                if 'lives' in message:
                    self.lives = message['lives']
                self.root.after(0, lambda m=message: self.show_waiting_room(m))
            else:
                msg = message.get('message', 'Impossible de rejoindre')
                self.root.after(0, lambda m=msg: messagebox.showerror("Erreur", m))
        
        elif action == 'session/player-joined':
            # Notification nouveau joueur
            new_player = message.get('pseudo', 'Unknown')
            player_count = message.get('playerCount', 0)
            print(f"{new_player} a rejoint la session ({player_count} joueurs)")
            
            if hasattr(self, 'players_listbox'):
                self.root.after(0, lambda: self.players_listbox.insert(tk.END, f"🎮 {new_player}"))
        
        elif action == 'session/started':
            countdown = message.get('countdown', 3)
            print(f"Session démarrée ! Countdown: {countdown}")
            self.root.after(0, lambda c=countdown: self.show_countdown(c))
        
        elif action == 'question/new':
            print(f"Nouvelle question reçue")
            self.root.after(0, lambda m=message: self.show_question(m))
        
        elif action == 'question/results':
            # Résultats intermédiaires
            print(f"Résultats de la question reçus")
            self.root.after(0, lambda m=message: self.show_question_results(m))
        
        elif action == 'joker/used':
            joker_type = message.get('type', '')
            
            if statut == '200':
                if joker_type == 'fifty':
                    removed = message.get('removedAnswers', [])
                    print(f"50/50 utilisé : réponses retirées = {removed}")
                    
                    def disable_buttons():
                        if hasattr(self, 'qcm_buttons') and len(self.qcm_buttons) > 0:
                            for idx in removed:
                                if 0 <= idx < len(self.qcm_buttons):
                                    btn = self.qcm_buttons[idx]
                                    btn.config(state='disabled', fg="red", disabledforeground="red")
                    
                    self.root.after(0, disable_buttons)
                    
                elif joker_type == 'skip':
                    # Pour le skip, on ne fait RIEN
                    # La question suivante arrive automatiquement via question/new
                    print("Skip joker reçu, attente de question/new...")
                    pass
            else:
                msg = message.get('message', 'Joker non disponible')
                self.root.after(0, lambda m=msg: messagebox.showerror("Erreur", m))

        elif action == 'chat/message':
            pseudo = message.get('pseudo', 'Unknown')
            text = message.get('message', '')
            timestamp = message.get('timestamp', None)
            self.root.after(0, lambda p=pseudo, t=text, ts=timestamp: self.append_chat_message(p, t, ts))

        elif action == 'chat/sent':
            pass


        elif action == 'session/player-eliminated':
            eliminated_player = message.get('pseudo', 'Un joueur')
            self.root.after(0, lambda p=eliminated_player: 
                messagebox.showinfo("Élimination", f"💀 {p} a été éliminé !"))
        
        elif action == 'session/finished':
            print(f"Partie terminée !")
            self.root.after(0, lambda m=message: self.show_final_results(m))
    
    def on_closing(self):
        # detruire la fenetre de chat si ouverte #
        if hasattr(self, 'chat_window'):
            try:
                self.chat_window.destroy()
            except Exception:
                pass
        self.client.disconnect()
        self.root.destroy()
    
    def run(self):
        self.root.mainloop()

if __name__ == "__main__":
    app = QuizNetGUI()
    app.run()
