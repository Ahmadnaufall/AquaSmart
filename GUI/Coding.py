import customtkinter as ctk
import serial
import threading
import time
from datetime import datetime
import random
import tkinter as tk

ph_value = 0.0
countdown = 10 * 1000
history = []
warning_active = False
warning_timeout = 0

ser = serial.serial_for_url('rfc2217://localhost:4000', baudrate=115200)

def serial_listener():
    global ph_value, countdown, history
    buffer = ""
    while True:
        if ser.in_waiting > 0:
            char = ser.read().decode(errors='ignore')
            if char in ['\n', '\r']:
                line = buffer.strip()
                buffer = ""
                if line.startswith("WARNING"):
                    show_warning("⚠️ pH Air Terlalu Tinggi!")
                elif "feeding ON" in line:
                    mode = "Otomatis" if "Auto" in line else "Manual"
                    now = datetime.now().strftime("%H:%M")
                    history.insert(0, (now, mode))
                    if len(history) > 5:
                        history.pop()
                    update_history()
                elif "feeding DONE" in line:
                    countdown = 10 * 1000
                else:
                    try:
                        ph_value = float(line)
                    except ValueError:
                        pass
            else:
                buffer += char
        time.sleep(0.05)

# === GUI SETUP ===
ctk.set_appearance_mode("light")
ctk.set_default_color_theme("blue")

app = ctk.CTk()
app.title("AQUA SMART UI")
app.geometry("440x600")
app.configure(fg_color="#ccf2ff")  # Light blue background

title = ctk.CTkLabel(app, text="🌊 AQUA SMART", font=("Arial", 22, "bold"), text_color="#006680")
title.pack(pady=(10, 8))

# === Countdown Frame ===
countdown_frame = ctk.CTkFrame(app, corner_radius=10, fg_color="#b3e6f7")
countdown_frame.pack(pady=5, padx=15, fill="x")

countdown_label = ctk.CTkLabel(countdown_frame, text="Countdown Pakan", font=("Arial", 12), text_color="#004466")
countdown_label.pack(pady=(8, 0))

countdown_time = ctk.CTkLabel(countdown_frame, text="00:00", font=("Arial", 22, "bold"), text_color="#008fb3")
countdown_time.pack(pady=(0, 10))

# === pH Frame ===
ph_frame = ctk.CTkFrame(app, corner_radius=10, fg_color="#b3e6f7")
ph_frame.pack(pady=5, padx=15, fill="x")

ph_label = ctk.CTkLabel(ph_frame, text="pH Air", font=("Arial", 12), text_color="#004466")
ph_label.pack(pady=(8, 0))

ph_value_label = ctk.CTkLabel(ph_frame, text="0.0", font=("Arial", 22, "bold"), text_color="#008fb3")
ph_value_label.pack(pady=(0, 10))

# === Warning Label ===
warning_label = ctk.CTkLabel(app, text="", font=("Arial", 12, "bold"), text_color="#cc0000")
warning_label.pack(pady=4)

def show_warning(msg):
    global warning_active, warning_timeout
    warning_label.configure(text=msg)
    warning_active = True
    warning_timeout = time.time() + 5

# === Notifikasi Panel ===
notif_frame = ctk.CTkFrame(app, corner_radius=10, fg_color="#b3e6f7")
notif_frame.pack(pady=5, padx=15, fill="x")

notif_label = ctk.CTkLabel(notif_frame, text="Notifikasi", font=("Arial", 12, "bold"), text_color="#004466")
notif_label.pack(pady=(6, 0))

icon_label = ctk.CTkLabel(notif_frame, text="🐟 🐠 🦐", font=("Arial", 22))
icon_label.pack(pady=4)

# === History Frame ===
history_frame = ctk.CTkFrame(app, corner_radius=10, fg_color="#b3e6f7")
history_frame.pack(pady=8, padx=15, fill="both", expand=True)

history_label = ctk.CTkLabel(history_frame, text="📋 Riwayat Pakan", font=("Arial", 12, "bold"), text_color="#004466")
history_label.pack(pady=(6, 2))

history_text = ctk.CTkLabel(history_frame, text="", justify="left", font=("Courier", 11), text_color="#00334d")
history_text.pack(pady=4, padx=10)

def update_history():
    display = ""
    for jam, mode in history:
        display += f"{jam}  {mode}\n"
    history_text.configure(text=display.strip())

# === Bubble Animation ===
canvas = tk.Canvas(app, bg="#ccf2ff", highlightthickness=0, height=100)
canvas.pack(fill="x", side="bottom")

bubbles = []

def create_bubble():
    x = random.randint(10, 420)
    size = random.randint(5, 12)
    bubble = canvas.create_oval(x, 100, x + size, 100 + size, fill="#66d9ff", outline="")
    bubbles.append((bubble, size))

def animate_bubbles():
    while True:
        for i, (bubble, size) in enumerate(bubbles):
            canvas.move(bubble, 0, -2)
            coords = canvas.coords(bubble)
            if coords[1] <= 0:
                canvas.delete(bubble)
                bubbles[i] = (None, 0)
        bubbles[:] = [(b, s) for b, s in bubbles if b is not None]
        if len(bubbles) < 10:
            create_bubble()
        time.sleep(0.05)

bubble_thread = threading.Thread(target=animate_bubbles, daemon=True)
bubble_thread.start()

# === UI LOOP ===
def update_ui():
    global countdown, warning_active
    seconds = countdown // 1000
    m, s = divmod(seconds, 60)
    countdown_time.configure(text=f"{m:02}:{s:02}")
    if countdown > 0:
        countdown -= 100

    ph_value_label.configure(text=f"{ph_value:.1f}")
    if ph_value > 8.0:
        show_warning("⚠️ pH Air Terlalu Tinggi!")

    if warning_active and time.time() > warning_timeout:
        warning_label.configure(text="")
        warning_active = False

    app.after(100, update_ui)

# === THREAD START ===
serial_thread = threading.Thread(target=serial_listener, daemon=True)
serial_thread.start()

app.after(100, update_ui)
app.mainloop()
