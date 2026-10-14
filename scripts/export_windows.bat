@echo off
rem ---------------------------------------------------------------------------
rem Pokemon Randomizer: fertigen Programmordner erstellen (Windows)
rem
rem Vorher in Qt Creator einmal das Projekt im Modus "Release" bauen.
rem Dann diese Datei per Doppelklick starten. Ergebnis:
rem   Export\Pokemon-Randomizer\PokemonRandomizer.exe   (alles, was zum Starten noetig ist)
rem   Export\Pokemon-Randomizer.zip                     (derselbe Ordner als ZIP zum Weitergeben)
rem ---------------------------------------------------------------------------
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0.."
for %%I in ("%ROOT%") do set "ROOT=%%~fI"
set "EXPORT=%ROOT%\Export"
set "TARGET=%EXPORT%\Pokemon-Randomizer"

echo.
echo  Pokemon Randomizer - Export
echo  ===========================
echo.

rem --- Neuestes Release-Build mit PokemonRandomizer.exe suchen ---
set "BUILD="
for /f "delims=" %%D in ('dir /b /ad /o-d "%ROOT%\build\*Release*" 2^>nul') do (
    if not defined BUILD if exist "%ROOT%\build\%%D\PokemonRandomizer.exe" set "BUILD=%ROOT%\build\%%D"
)
if not defined BUILD (
    echo  FEHLER: Kein Release-Build gefunden.
    echo  Bitte in Qt Creator unten links "Release" auswaehlen und das Projekt bauen,
    echo  danach dieses Skript erneut starten.
    goto :fail
)
echo  Release-Build: %BUILD%

rem --- Zielordner neu anlegen (alte Exporte werden ersetzt) ---
if exist "%TARGET%" rmdir /s /q "%TARGET%"
if exist "%TARGET%" (
    echo  FEHLER: Der alte Export-Ordner konnte nicht geloescht werden.
    echo  Ist der Randomizer aus diesem Ordner noch geoeffnet?
    goto :fail
)
mkdir "%TARGET%" || goto :fail

rem --- Programm und Qt-Laufzeit ---
echo  Kopiere Programm und Qt-Bibliotheken ...
copy /y "%BUILD%\PokemonRandomizer.exe" "%TARGET%\" >nul || goto :fail
copy /y "%BUILD%\*.dll" "%TARGET%\" >nul 2>nul
for %%P in (generic iconengines imageformats networkinformation platforms styles tls translations) do (
    if exist "%BUILD%\%%P" call :copydir "%BUILD%\%%P" "%TARGET%\%%P" || goto :fail
)

rem Falls im Build-Ordner noch keine Qt-Bibliotheken liegen: windeployqt ausfuehren
if not exist "%TARGET%\Qt6Core.dll" (
    call :findqt
    if not defined WINDEPLOYQT (
        echo  FEHLER: Qt-Bibliotheken fehlen und windeployqt wurde nicht gefunden.
        echo  Bitte das Projekt in Qt Creator im Modus "Release" neu bauen.
        goto :fail
    )
    echo  Fuehre windeployqt aus ...
    "!WINDEPLOYQT!" --release "%TARGET%\PokemonRandomizer.exe" >nul || goto :fail
)

rem --- Daten des Randomizers ---
echo  Kopiere Spieldaten und Bilder ...
call :copydir "%ROOT%\SV_FLATBUFFERS" "%TARGET%\SV_FLATBUFFERS" /xf *_array.json data.json || goto :fail
call :copydir "%ROOT%\assets" "%TARGET%\assets" || goto :fail
call :copydir "%ROOT%\thirdparty\flatbuffers\windows" "%TARGET%\thirdparty\flatbuffers\windows" || goto :fail
mkdir "%TARGET%\logs" 2>nul
type nul > "%TARGET%\logs\logs.log"
if exist "%ROOT%\LICENSE" copy /y "%ROOT%\LICENSE" "%TARGET%\LICENSE.txt" >nul

rem --- ZIP zum Weitergeben ---
echo  Erstelle ZIP-Datei ...
if exist "%EXPORT%\Pokemon-Randomizer.zip" del /q "%EXPORT%\Pokemon-Randomizer.zip"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%TARGET%' -DestinationPath '%EXPORT%\Pokemon-Randomizer.zip' -Force" >nul 2>nul
if not exist "%EXPORT%\Pokemon-Randomizer.zip" echo  Hinweis: Die ZIP-Datei konnte nicht erstellt werden, der Ordner ist trotzdem fertig.

echo.
echo  Fertig!
echo  Programmordner: %TARGET%
echo  Starte darin PokemonRandomizer.exe - Qt Creator wird dafuer nicht mehr gebraucht.
echo.
start "" explorer "%TARGET%"
pause
exit /b 0

rem ---------------------------------------------------------------------------
:copydir
rem %1 Quelle, %2 Ziel, weitere Parameter gehen an robocopy (z. B. /xf ...)
set "SRC=%~1"
set "DST=%~2"
shift
shift
set "EXTRA="
:copydir_args
if "%~1"=="" goto :copydir_run
set "EXTRA=!EXTRA! %1"
shift
goto :copydir_args
:copydir_run
robocopy "%SRC%" "%DST%" /e /njh /njs /nfl /ndl /np !EXTRA! >nul
if errorlevel 8 (
    echo  FEHLER beim Kopieren von %SRC%
    exit /b 1
)
exit /b 0

:findqt
set "WINDEPLOYQT="
for /f "delims=" %%W in ('where windeployqt 2^>nul') do if not defined WINDEPLOYQT set "WINDEPLOYQT=%%W"
if defined WINDEPLOYQT exit /b 0
for /f "delims=" %%Q in ('dir /b /ad /o-n "C:\Qt\6*" 2^>nul') do (
    for /f "delims=" %%M in ('dir /b /ad "C:\Qt\%%Q\mingw*" 2^>nul') do (
        if not defined WINDEPLOYQT if exist "C:\Qt\%%Q\%%M\bin\windeployqt.exe" (
            set "WINDEPLOYQT=C:\Qt\%%Q\%%M\bin\windeployqt.exe"
            set "PATH=C:\Qt\%%Q\%%M\bin;!PATH!"
        )
    )
)
for /f "delims=" %%T in ('dir /b /ad /o-n "C:\Qt\Tools\mingw*" 2^>nul') do (
    if exist "C:\Qt\Tools\%%T\bin\g++.exe" set "PATH=C:\Qt\Tools\%%T\bin;!PATH!"
)
exit /b 0

:fail
echo.
echo  Der Export wurde abgebrochen.
pause
exit /b 1
