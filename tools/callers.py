import subprocess, sys

addrs = ['435b30','435c50','436080','436090','4360a0','4360b0','4360d0','4360f0','436110','4372f0','437320','4377d0','437810']
for a in addrs:
    r = subprocess.run(['.venv\\Scripts\\python.exe', 'C:\\Users\\Gengaroshi\\AppData\\Local\\Temp\\opencode\\findptr.py', a], capture_output=True, text=True, cwd='C:\\Locoland Decomp\\Locoland-src')
    out = r.stdout.strip().replace('\n',' | ')
    print(f'{a}: {out}')
