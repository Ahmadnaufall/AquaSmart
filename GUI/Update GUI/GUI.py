import ttkbootstrap as ttk
from ttkbootstrap.constants import *
from tkinter import Text, messagebox
from datetime import datetime
import random
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from PIL import Image, ImageTk
from pathlib import Path

class AquaSmartApp:
    MAX_VOLUME_ML = 1000  # Kapasitas maksimal volume pakan dalam ml

    def _init_(self, root):
        self.root = root
        self.root.title("Aqua Smart - Monitoring pH & Pakan")
        self.root.geometry("1440x900")
        self.root.minsize(1200, 700)

        # Variables
        self.ph_value = ttk.StringVar(value="--")
        self.volume_percent = ttk.DoubleVar(value=100)  # Volume pakan dalam persen (0-100)
        self.volume_warning = ttk.StringVar(value="")
        
        self.timer_default = 10800  # 3 jam default
        self.timer_seconds = self.timer_default
        self.timer_running = False
        
        # Data grafik pH
        self.ph_data = [7.0]*60
        
        self.setup_styles()
        self.create_widgets()
        self.update_ph()
        self.start_timer()
        self.update_volume()
        
    def setup_styles(self):
        style = ttk.Style()
        style.configure("Header.TLabel", font=("BerkshireSwash", 36), foreground="#007acc")
        style.configure("Large.TLabel", font=("Segoe UI", 48), foreground="#007acc")
        style.configure("Timer.TLabel", font=("Segoe UI", 36), foreground="#007acc")
        style.configure("Warning.TLabel", font=("Segoe UI", 12, "bold"), foreground="red")
        style.configure("VolumeLabel.TLabel", font=("Segoe UI", 16), foreground="#000")
        
    def relative_to_assets(self, filename):
        return Path(_file_).parent / filename
    
    def create_widgets(self):
        header = ttk.Label(self.root, text="AQUA SMART", style="Header.TLabel", anchor="center")
        header.grid(row=0, column=0, columnspan=3, pady=10, sticky="ew")

        self.root.grid_columnconfigure(0, weight=1)
        self.root.grid_columnconfigure(1, weight=1)
        self.root.grid_columnconfigure(2, weight=1)

        left_frame = ttk.Frame(self.root)
        left_frame.grid(row=1, column=0, sticky="nsew", padx=(20,10), pady=10)
        
        ph_label = ttk.Label(left_frame, textvariable=self.ph_value, style="Large.TLabel")
        ph_label.pack(pady=5)

        self.fig, self.ax = plt.subplots(figsize=(7,3.5))
        self.line, = self.ax.plot(self.ph_data, color="#007acc", linewidth=2)
        self.ax.set_ylim(0, 10)
        self.ax.set_title("pH Realtime")
        self.ax.set_ylabel("pH Value")
        self.ax.grid(True, linestyle="--", alpha=0.5)
        self.canvas = FigureCanvasTkAgg(self.fig, master=left_frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        right_frame = ttk.Frame(self.root)
        right_frame.grid(row=1, column=1, sticky="nsew", padx=(10,20), pady=10)

        vol_label = ttk.Label(right_frame, text="Volume Pakan:")
        vol_label.pack(anchor="w", pady=(0,5))

        # Progress bar dan label persen
        self.volume_progress = ttk.Progressbar(right_frame, length=300, maximum=100)
        self.volume_progress.pack(pady=(0,5))
        
        self.volume_percent_label = ttk.Label(right_frame, text="100%", style="VolumeLabel.TLabel", foreground="black")
        self.volume_percent_label.pack()  # Tampilkan di bawah progress bar agar terlihat jelas

        self.volume_warning_label = ttk.Label(right_frame, textvariable=self.volume_warning, style="Warning.TLabel")
        self.volume_warning_label.pack()

        image_path = self.relative_to_assets("ikon volume ikan.jpg")
        pil_image = Image.open(image_path)
        pil_image = pil_image.resize((300, 300), Image.LANCZOS)
        self.icon_volume_image = ImageTk.PhotoImage(pil_image)
        image_label = ttk.Label(right_frame, image=self.icon_volume_image)
        image_label.pack(pady=(5, 15))
        
        self.timer_label = ttk.Label(right_frame, text=self.format_time(self.timer_seconds), style="Timer.TLabel")
        self.timer_label.pack()
        
        edit_frame = ttk.Frame(right_frame)
        edit_frame.pack(pady=15, fill="x")
        
        ttk.Label(edit_frame, text="Jam:").grid(row=0, column=0)
        self.hour_var = ttk.StringVar(value="3")
        self.hour_spin = ttk.Spinbox(edit_frame, from_=0, to=23, width=5, textvariable=self.hour_var, style="TSpinbox")
        self.hour_spin.grid(row=0, column=1, padx=5)
        
        ttk.Label(edit_frame, text="Menit:").grid(row=0, column=2)
        self.min_var = ttk.StringVar(value="0")
        self.min_spin = ttk.Spinbox(edit_frame, from_=0, to=59, width=5, textvariable=self.min_var, style="TSpinbox")
        self.min_spin.grid(row=0, column=3, padx=5)
        
        ttk.Label(edit_frame, text="Detik:").grid(row=0, column=4)
        self.sec_var = ttk.StringVar(value="0")
        self.sec_spin = ttk.Spinbox(edit_frame, from_=0, to=59, width=5, textvariable=self.sec_var, style="TSpinbox")
        self.sec_spin.grid(row=0, column=5, padx=5)
        
        edit_btn = ttk.Button(edit_frame, text="Set Timer", command=self.set_timer, bootstyle=SUCCESS)
        edit_btn.grid(row=0, column=6, padx=10)
        
        log_frame = ttk.LabelFrame(self.root, text="Log Aktivitas")
        log_frame.grid(row=2, column=0, columnspan=3, sticky="nsew", padx=20, pady=(5,20))
        
        self.log_text = Text(log_frame, height=10, font=("Consolas", 11))
        self.log_text.pack(fill="both", expand=True)
        
        self.root.grid_rowconfigure(1, weight=1)
        self.root.grid_rowconfigure(2, weight=0)
        self.root.grid_columnconfigure(0, weight=3)
        self.root.grid_columnconfigure(1, weight=1)
        self.root.grid_columnconfigure(2, weight=0)
    
    def format_time(self, seconds):
        hrs, rem = divmod(seconds, 3600)
        mins, secs = divmod(rem, 60)
        return f"{hrs:02d}:{mins:02d}:{secs:02d}"
    
    def update_ph(self):
        new_ph = round(random.uniform(1, 10), 2)
        self.ph_value.set(f"{new_ph}")
        self.ph_data.append(new_ph)
        if len(self.ph_data) > 60:
            self.ph_data.pop(0)
        self.line.set_ydata(self.ph_data)
        self.line.set_xdata(range(len(self.ph_data)))
        self.ax.set_xlim(0, len(self.ph_data))
        self.canvas.draw_idle()

        if new_ph < 6.5:
            self.log_warning("pH air terlalu rendah, SEGERA GANTI AIR!!!")
        elif new_ph > 8.5:
            self.log_warning("pH air terlalu tinggi, SEGERA GANTI AIR!!!")
    
        self.root.after(1000, self.update_ph)

    def log_warning(self, message):
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.log_text.insert("end", f"{timestamp} - WARNING: {message}\n")
        self.log_text.see("end")

    def update_volume(self):
        volume_ml = random.uniform(0, self.MAX_VOLUME_ML)
        volume_percent = (volume_ml / self.MAX_VOLUME_ML) * 100
        
        self.volume_percent.set(round(volume_percent, 2))
        self.volume_progress['value'] = volume_percent
        self.volume_percent_label.config(text=f"{volume_percent:.2f}%")

        if volume_percent <= 10:
            self.volume_warning.set("Pakan terlalu sedikit!")
        else:
            self.volume_warning.set("")

        self.root.after(5000, self.update_volume)
        
    def start_timer(self):
        self.timer_running = True
        self._timer_tick()
        
    def _timer_tick(self):
        if not self.timer_running:
            return
        if self.timer_seconds <= 0:
            self.log_activity("Timer habis: Pemberian pakan otomatis dijalankan")
            self.timer_seconds = self.timer_default
            self.log_activity(f"Timer di-reset ke {self.format_time(self.timer_seconds)}")
        self.timer_label.config(text=self.format_time(self.timer_seconds))
        self.timer_seconds -= 1
        self.root.after(1000, self._timer_tick)
        
    def set_timer(self):
        try:
            h = int(self.hour_var.get())
            m = int(self.min_var.get())
            s = int(self.sec_var.get())
            total = h * 3600 + m * 60 + s
            if total <= 0:
                messagebox.showwarning("Input Timer", "Timer harus lebih dari 0 detik.")
                return
            self.timer_seconds = total
            self.timer_default = total
            self.timer_label.config(text=self.format_time(self.timer_seconds))
            self.log_activity(f"Timer diset manual ke {h} jam {m} menit {s} detik")
        except ValueError:
            messagebox.showerror("Input Timer", "Masukkan nilai jam, menit, dan detik yang valid.")
    
    def log_activity(self, message):
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.log_text.insert("end", f"{timestamp} - {message}\n")
        self.log_text.see("end")

if _name_ == "_main_":
    root = ttk.Window(themename="cosmo")
    app = AquaSmartApp(root)
    root.mainloop()
