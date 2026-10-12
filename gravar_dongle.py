"""Grava o dongle S3 ou o fone C3 em modo boot e apaga o canal salvo."""

import os
import queue
import subprocess
import sys
import threading
import tkinter as tk
from pathlib import Path
from tkinter import messagebox, ttk

IDF_PYTHON = Path(r"H:\espressif\python_env\idf5.4_py3.14_env\Scripts\python.exe")
ROOT = Path(__file__).resolve().parent
BUILD = {
    "esp32s3": ROOT / "firmware" / "dongle_s3" / "build",
    "esp32c3": ROOT / "firmware" / "headset_c3" / "build",
}
IMAGES = {
    "esp32s3": {
        "size": "16MB",
        "name": "dongle S3",
        "files": [
            ("0x0", "bootloader/bootloader.bin"),
            ("0x8000", "partition_table/partition-table.bin"),
            ("0x10000", "dongle_s3.bin"),
        ],
    },
    "esp32c3": {
        "size": "4MB",
        "name": "fone C3",
        "files": [
            ("0x0", "bootloader/bootloader.bin"),
            ("0x8000", "partition_table/partition-table.bin"),
            ("0x10000", "headset_c3.bin"),
        ],
    },
}


def ensure_idf_python() -> None:
    try:
        import esptool  # noqa: F401
        return
    except ImportError:
        pass
    if IDF_PYTHON.exists() and Path(sys.executable) != IDF_PYTHON:
        os.execv(str(IDF_PYTHON), [str(IDF_PYTHON), str(Path(__file__).resolve()), *sys.argv[1:]])
    raise SystemExit("Não achei o Python do ESP-IDF em H:\\espressif.")


def list_ports() -> list[str]:
    from serial.tools import list_ports

    return [port.device for port in list_ports.comports()]


class Flasher(tk.Tk):
    def __init__(self) -> None:
        super().__init__()
        self.title("Gravar firmware")
        self.geometry("720x520")
        self.minsize(640, 460)
        self._queue: queue.Queue[tuple[str, str]] = queue.Queue()
        self._busy = False

        text = (
            "Desligue e esfrie a placa antes de gravar. Use só o conector que vira porta COM "
            "no modo boot: no dongle é o UART do CH343, não o USB do áudio. Segure BOOT, "
            "toque em RESET e solte BOOT. A gravação apaga o canal salvo e volta para o canal 6."
        )
        ttk.Label(self, text=text, wraplength=680, padding=12).pack(fill="x")

        row = ttk.Frame(self, padding=(12, 0))
        row.pack(fill="x")
        ttk.Label(row, text="Porta").pack(side="left")
        self.port = ttk.Combobox(row, width=18, state="readonly")
        self.port.pack(side="left", padx=8)
        ttk.Button(row, text="Atualizar portas", command=self.refresh).pack(side="left")
        self.button = ttk.Button(row, text="Gravar", command=self.start)
        self.button.pack(side="right")

        self.log = tk.Text(self, height=20, wrap="word", font=("Consolas", 10))
        self.log.pack(fill="both", expand=True, padx=12, pady=12)
        self.refresh()
        self.after(100, self._drain)
        self.write("Esperando a placa em modo boot.\n")

    def refresh(self) -> None:
        ports = list_ports()
        self.port["values"] = ports
        if ports and self.port.get() not in ports:
            self.port.set(ports[0])
        if not ports:
            self.port.set("")

    def write(self, text: str) -> None:
        self.log.insert("end", text)
        self.log.see("end")

    def _drain(self) -> None:
        try:
            while True:
                kind, text = self._queue.get_nowait()
                if kind == "log":
                    self.write(text)
                elif kind == "done":
                    self._busy = False
                    self.button.state(["!disabled"])
                    self.write(text)
        except queue.Empty:
            pass
        self.after(100, self._drain)

    def start(self) -> None:
        if self._busy:
            return
        port = self.port.get().strip()
        if not port:
            messagebox.showwarning("Porta", "Nenhuma porta COM apareceu. Entre em modo boot e atualize.")
            return
        self._busy = True
        self.button.state(["disabled"])
        self.write(f"\n--- {port} ---\n")
        threading.Thread(target=self._run, args=(port,), daemon=True).start()

    def _run(self, port: str) -> None:
        try:
            chip = self._identify(port)
            image = IMAGES[chip]
            build = BUILD[chip]
            missing = [name for _, name in image["files"] if not (build / name).exists()]
            if missing:
                raise RuntimeError("Falta o arquivo de firmware: " + ", ".join(missing))
            self._queue.put(("log", f"Chip {chip}. Apagando a memória do {image['name']}.\n"))
            self._esptool(port, chip, ["erase_flash"])
            args = [
                "write_flash",
                "--flash_mode", "dio",
                "--flash_freq", "80m",
                "--flash_size", image["size"],
            ]
            for offset, name in image["files"]:
                args.extend([offset, str(build / name)])
            self._queue.put(("log", "Gravando o firmware. O canal volta para 6.\n"))
            self._esptool(port, chip, args, after=True)
            self._queue.put(("done", "Pronto. Solte a placa, tire do modo boot e ligue de novo.\n"))
        except Exception as exc:
            self._queue.put(("done", f"Falhou: {exc}\n"))

    def _identify(self, port: str) -> str:
        output = self._esptool(port, "auto", ["chip_id"])
        lowered = output.lower()
        if "esp32-c3" in lowered or "esp32c3" in lowered:
            return "esp32c3"
        if "esp32-s3" in lowered or "esp32s3" in lowered:
            return "esp32s3"
        raise RuntimeError("O chip não é o dongle S3 nem o fone C3. Nada foi apagado.")

    def _esptool(self, port: str, chip: str, args: list[str], after: bool = False) -> str:
        command = [
            sys.executable, "-m", "esptool",
            "--chip", chip,
            "-p", port,
            "-b", "460800",
            "--before", "default_reset",
            "--after", "hard_reset" if after else "no_reset",
            *args,
        ]
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        captured: list[str] = []
        assert process.stdout is not None
        for line in process.stdout:
            captured.append(line)
            self._queue.put(("log", line))
        code = process.wait()
        output = "".join(captured)
        if code != 0:
            raise RuntimeError(f"esptool saiu com código {code}")
        return output


if __name__ == "__main__":
    ensure_idf_python()
    Flasher().mainloop()
