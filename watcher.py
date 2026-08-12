import time
import pyttsx3

LOG_FILE = "announce.log"

def speak(text):
    print(f"Speaking: {text}")
    engine = pyttsx3.init()          # fresh engine every call
    engine.setProperty('rate', 150)
    engine.say(text)
    engine.runAndWait()
    engine.stop()
    del engine

def watch():
    with open(LOG_FILE, "r") as f:
        f.seek(0, 2)
        print("Watching for announcements... (Ctrl+C to stop)")
        while True:
            line = f.readline()
            if not line:
                time.sleep(0.2)
                continue
            line = line.strip()
            if line.startswith("ANNOUNCE:"):
                text = line.replace("ANNOUNCE:", "").strip()
                speak(text)

if __name__ == "__main__":
    watch()