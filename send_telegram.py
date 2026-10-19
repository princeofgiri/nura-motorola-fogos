#!/usr/bin/env python3
import os
import sys
import json
import urllib.request

def get_bot_token():
    token = os.environ.get("TELEGRAM_BOT_TOKEN")
    if token:
        return token.strip()
    telebot_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "telebot")
    if os.path.exists(telebot_path):
        try:
            with open(telebot_path, "r") as f:
                for line in f:
                    line = line.strip()
                    if ":" in line and not line.startswith("HTTP") and not line.startswith("sender") and not line.startswith("receiver"):
                        return line
        except Exception:
            pass
    return ""

BOT_TOKEN = get_bot_token()
TARGET_USER = os.environ.get("TELEGRAM_TARGET_USER", "princeofgiri")

def send_message(chat_id, text):
    if not BOT_TOKEN:
        print("[ERROR] No bot token found. Set TELEGRAM_BOT_TOKEN or provide local telebot file.")
        return False
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/sendMessage"
    payload = json.dumps({"chat_id": chat_id, "text": text, "parse_mode": "Markdown"}).encode('utf-8')
    req = urllib.request.Request(url, data=payload, headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req) as resp:
            res = json.loads(resp.read().decode('utf-8'))
            if res.get("ok"):
                print(f"[SUCCESS] Message sent to chat_id {chat_id}")
                return True
            else:
                print(f"[FAIL] Error: {res}")
                return False
    except Exception as e:
        print(f"[ERROR] Exception sending message: {e}")
        return False

DEFAULT_CHAT_ID = 3787545

def get_chat_id():
    url = f"https://api.telegram.org/bot{BOT_TOKEN}/getUpdates"
    try:
        with urllib.request.urlopen(url) as resp:
            data = json.loads(resp.read().decode('utf-8'))
            if not data.get("ok"):
                return DEFAULT_CHAT_ID
            updates = data.get("result", [])
            for u in reversed(updates):
                msg = u.get("message") or u.get("channel_post") or u.get("my_chat_member")
                if not msg:
                    continue
                chat = msg.get("chat", {})
                user = msg.get("from", {})
                if user.get("username", "").lower() == TARGET_USER.lower() or chat.get("username", "").lower() == TARGET_USER.lower():
                    return chat.get("id")
                # Fallback to any recent private chat
                if chat.get("type") == "private":
                    return chat.get("id")
    except Exception as e:
        print(f"[ERROR] Exception fetching updates: {e}")
    return DEFAULT_CHAT_ID

def main():
    message = sys.argv[1] if len(sys.argv) > 1 else "Halo! Konfigurasi seluler Motorola Moto G34 5G selesai."
    chat_id = get_chat_id()
    if chat_id:
        send_message(chat_id, message)
    else:
        print("[WARNING] Could not find chat_id from getUpdates. User must start chat with @bojong_bot.")

if __name__ == "__main__":
    main()
