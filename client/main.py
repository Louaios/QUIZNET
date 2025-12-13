#!/usr/bin/env python3
"""
Point d'entrée principal du client QuizNet
Lance l'interface graphique
"""
from gui import QuizNetGUI

def main():
    app = QuizNetGUI()
    app.run()

if __name__ == "__main__":
    main()
