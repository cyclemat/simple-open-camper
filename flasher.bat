@echo off
rem Simple Open Camper – lokaler Web-Flasher (Panels und Satelliten per USB aus dem Browser)
cd /d "%~dp0"
where py >nul 2>nul && (py -3 tools\flasher.py) || (python tools\flasher.py)
pause
