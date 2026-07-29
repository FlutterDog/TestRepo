# Команды генерации Doxygen

Рабочая папка проекта:

```bat
cd /d C:\DoxygenFolder
```

Создать папки и запустить Doxygen:

```bat
if not exist docs mkdir docs
if not exist docs\doxygen mkdir docs\doxygen
doxygen Doxyfile
```

Запустить с логом:

```bat
if not exist docs mkdir docs
if not exist docs\doxygen mkdir docs\doxygen
doxygen Doxyfile > docs\doxygen\doxygen_build.log 2>&1
```

Открыть результат:

```bat
start docs\doxygen\html\index.html
```

Вариант через готовый bat-файл:

```bat
cd /d C:\DoxygenFolder
build_doxygen.bat
```

Если `doxygen.exe` не найден, добавить папку установки Doxygen в PATH или запускать Doxygen из установленного ярлыка/GUI.
