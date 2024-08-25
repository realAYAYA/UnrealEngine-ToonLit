@echo off

call createenv.bat

python manage.py runserver 0.0.0.0:8000
