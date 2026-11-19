#!/usr/bin/env bash
set -e

APP_NAME="qGlobalManager"
APP_DISPLAY_NAME="Global Manager"
APP_VERSION="0.1.0"

BUILD_DIR="build"
DIST_LINUX_DIR="dist/linux"
DIST_WIN_DIR="dist/windows"
LINUX_INSTALL_DIR="/opt/${APP_NAME}"
WIN_INSTALL_DIR="C:\\Programs_Files\\${APP_NAME}"

# Valores por defecto
DO_CLEAN=false
BUILD_MODE="release"
DO_INSTALL=false
DO_DEPLOY_LINUX=false
DO_DEPLOY_WIN=false
DO_RUN=false
AS_ROOT=false

# Colores para la salida en terminal
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

show_help() {
    cat << EOF
Uso: ./build.sh [OPCIONES]

Script de compilación, instalación, empaquetado y ejecución para ${APP_DISPLAY_NAME} (${APP_NAME}).
Las opciones son combinables entre sí.

Opciones:
  -h, --help               Muestra esta ayuda y sale.
  -c, --clean              Limpia compilaciones previas ('${BUILD_DIR}', Makefiles y cachés de qmake).
  -d, --debug              Compila en modo Debug para desarrollo (salida en '${BUILD_DIR}/').
  -r, --release            Compila en modo Release optimizado (por defecto).
  -i, --install            Instala la aplicación en '${LINUX_INSTALL_DIR}/' (Linux).
      --make-deploy-linux  Compila en Release y genera '${DIST_LINUX_DIR}/${APP_NAME}-<arch>.run'.
      --make-deploy-win    Compila en Release y genera '${DIST_WIN_DIR}/${APP_NAME}-<arch>.exe'.
  -x, --run                Ejecuta la aplicación tras compilar (ignorado en modos --make-deploy-*).
      --root               Ejecuta la aplicación como root (implica --run, ignorado en --make-deploy-*).

Variables de entorno opcionales:
  QMAKE      Ruta explícita a qmake para compilación nativa (ej. QMAKE=qmake6 ./build.sh).
  WIN_QMAKE  Ruta explícita a qmake de MinGW/MXE para compilación cruzada a Windows desde Linux.

Ejemplos:
  ./build.sh --clean --debug -x
  ./build.sh --clean --release --install
  ./build.sh --clean --make-deploy-linux
  ./build.sh --clean --make-deploy-win
EOF
}

# Parseo de argumentos combinables
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            show_help
            exit 0
            ;;
        -c|--clean)
            DO_CLEAN=true
            shift
            ;;
        -d|--debug)
            BUILD_MODE="debug"
            shift
            ;;
        -r|--release)
            BUILD_MODE="release"
            shift
            ;;
        -i|--install)
            DO_INSTALL=true
            shift
            ;;
        --make-deploy-linux)
            DO_DEPLOY_LINUX=true
            shift
            ;;
        --make-deploy-win)
            DO_DEPLOY_WIN=true
            shift
            ;;
        -x|--run)
            DO_RUN=true
            shift
            ;;
        --root)
            AS_ROOT=true
            DO_RUN=true
            shift
            ;;
        *)
            echo -e "${RED}[Error] Opción desconocida: $1${NC}"
            show_help
            exit 1
            ;;
    esac
done

# Verificar existencia del archivo .pro
if [ ! -f "${APP_NAME}.pro" ]; then
    echo -e "${RED}[Error] No se encontró '${APP_NAME}.pro' en el directorio actual.${NC}"
    exit 1
fi

# Regla de consigna: en modos --make-deploy-* siempre se compila en Release y NO se lanza la app
if [ "$DO_DEPLOY_LINUX" = true ] || [ "$DO_DEPLOY_WIN" = true ]; then
    BUILD_MODE="release"
    if [ "$DO_RUN" = true ]; then
        echo -e "${YELLOW}[Aviso] Se omite la ejecución automática porque se solicitó --make-deploy-*.${NC}"
        DO_RUN=false
        AS_ROOT=false
    fi
fi

# Detectar sistema operativo anfitrión
HOST_OS="$(uname -s)"
IS_WINDOWS_HOST=false
case "${HOST_OS}" in
    MINGW*|MSYS*|CYGWIN*)
        IS_WINDOWS_HOST=true
        ;;
esac

CORES=$(nproc 2>/dev/null || echo 2)

# Detectar qmake nativo (prioriza override QMAKE, luego Qt5 en Slackware 15, con fallback a qmake/qmake6)
detect_native_qmake() {
    if [ -n "${QMAKE:-}" ] && command -v "${QMAKE}" &> /dev/null; then
        echo "${QMAKE}"
    elif command -v qmake-qt5 &> /dev/null; then
        echo "qmake-qt5"
    elif command -v qmake &> /dev/null; then
        echo "qmake"
    elif command -v qmake6 &> /dev/null; then
        echo "qmake6"
    else
        echo ""
    fi
}

# Detectar qmake para Windows (nativo en MSYS2/MinGW o cruzado vía MXE en Linux)
detect_win_qmake() {
    if [ -n "${WIN_QMAKE:-}" ] && command -v "${WIN_QMAKE}" &> /dev/null; then
        echo "${WIN_QMAKE}"
        return
    fi

    if [ "$IS_WINDOWS_HOST" = true ]; then
        detect_native_qmake
        return
    fi

    for candidate in \
        x86_64-w64-mingw32.shared-qmake-qt5 \
        x86_64-w64-mingw32.shared-qmake-qt6 \
        i686-w64-mingw32.shared-qmake-qt5 \
        mingw64-qmake-qt5 \
        mingw64-qmake-qt6; do
        if command -v "${candidate}" &> /dev/null; then
            echo "${candidate}"
            return
        fi
    done

    echo ""
}

# Copiar iconos y documentación al directorio destino
copy_project_assets() {
    local dest_root="$1"

    mkdir -p "${dest_root}/documentation" "${dest_root}/icons"

    if [ -d "documentation" ] && [ "$(ls -A documentation 2>/dev/null)" ]; then
        cp -r documentation/. "${dest_root}/documentation/"
    fi

    if [ -d "resources/icons" ] && [ "$(ls -A resources/icons 2>/dev/null)" ]; then
        cp -r resources/icons/. "${dest_root}/icons/"
    elif [ -d "icons" ] && [ "$(ls -A icons 2>/dev/null)" ]; then
        cp -r icons/. "${dest_root}/icons/"
    fi
}

# Generar archivo .desktop para Linux
generate_desktop_file() {
    local dest_file="$1"
    local icon_value="applications-office"

    if [ -f "resources/icons/${APP_NAME}.svg" ] || [ -f "icons/${APP_NAME}.svg" ]; then
        icon_value="${LINUX_INSTALL_DIR}/icons/${APP_NAME}.svg"
    elif [ -f "resources/icons/${APP_NAME}.png" ] || [ -f "icons/${APP_NAME}.png" ]; then
        icon_value="${LINUX_INSTALL_DIR}/icons/${APP_NAME}.png"
    fi

    cat > "${dest_file}" << EOF
[Desktop Entry]
Version=${APP_VERSION}
Type=Application
Name=${APP_DISPLAY_NAME}
GenericName=Administración de Comercios y Consultorios
Comment=Sistema de administración general de comercios y consultorios en Qt/C++
Exec=${LINUX_INSTALL_DIR}/bin/${APP_NAME}
Icon=${icon_value}
Terminal=false
Categories=Office;Finance;Qt;
StartupNotify=true
EOF
}

# Compilar con qmake en un directorio dado
compile_project() {
    local qmake_bin="$1"
    local out_dir="$2"
    local mode="$3"
    local prefix="$4"
    local make_cmd="make"

    if [ "$IS_WINDOWS_HOST" = true ] && command -v mingw32-make &> /dev/null; then
        make_cmd="mingw32-make"
    fi

    mkdir -p "${out_dir}"
    local qt_ver
    qt_ver=$("${qmake_bin}" -query QT_VERSION 2>/dev/null || echo "desconocida")

    echo -e "${GREEN}==> Configurando ${APP_NAME} con ${qmake_bin} (Qt ${qt_ver} | Modo: ${mode})...${NC}"
    "${qmake_bin}" "${APP_NAME}.pro" \
        "CONFIG+=${mode}" \
        "PREFIX=${prefix}" \
        "DESTDIR=." \
        -o "${out_dir}/Makefile"

    echo -e "${GREEN}==> Compilando con ${CORES} hilos...${NC}"
    "${make_cmd}" -C "${out_dir}" -j"${CORES}"
}

# ==============================================================================
# 1. Limpieza previa si se solicitó (--clean)
# ==============================================================================
if [ "$DO_CLEAN" = true ]; then
    echo -e "${YELLOW}==> Limpiando archivos de compilación previos...${NC}"
    rm -rf "${BUILD_DIR}" Makefile Makefile.Debug Makefile.Release .qmake.stash
fi

mkdir -p "${BUILD_DIR}"

# ==============================================================================
# 2. Empaquetado autocontenido para Linux (--make-deploy-linux)
# ==============================================================================
if [ "$DO_DEPLOY_LINUX" = true ]; then
    if [ "$IS_WINDOWS_HOST" = true ]; then
        echo -e "${RED}[Error] --make-deploy-linux debe ejecutarse en un entorno Linux.${NC}"
        exit 1
    fi

    QMAKE_BIN=$(detect_native_qmake)
    if [ -z "${QMAKE_BIN}" ]; then
        echo -e "${RED}[Error] No se encontró qmake (qmake-qt5, qmake o qmake6) en el sistema.${NC}"
        exit 1
    fi

    compile_project "${QMAKE_BIN}" "${BUILD_DIR}" "release" "${LINUX_INSTALL_DIR}"

    BIN_PATH="${BUILD_DIR}/${APP_NAME}"
    if [ ! -f "${BIN_PATH}" ]; then
        echo -e "${RED}[Error] No se encontró el ejecutable compilado en '${BIN_PATH}'.${NC}"
        exit 1
    fi

    ARCH=$(uname -m)
    STAGE_DIR="${BUILD_DIR}/deploy_stage_linux"
    mkdir -p "${DIST_LINUX_DIR}"
    RUN_OUTPUT="${DIST_LINUX_DIR}/${APP_NAME}-${ARCH}.run"

    echo -e "${CYAN}==> Preparando paquete autocontenido Linux (${ARCH})...${NC}"
    rm -rf "${STAGE_DIR}"
    mkdir -p "${STAGE_DIR}/bin" "${STAGE_DIR}/libs" "${STAGE_DIR}/plugins"

    # Copiar ejecutable en bin/ (estructura de producción) y assets
    cp "${BIN_PATH}" "${STAGE_DIR}/bin/${APP_NAME}"
    strip --strip-unneeded "${STAGE_DIR}/bin/${APP_NAME}" 2>/dev/null || true
    copy_project_assets "${STAGE_DIR}"

    # Generar qt.conf apuntando a ../plugins y ../libs
    cat > "${STAGE_DIR}/bin/qt.conf" << EOF
[Paths]
Prefix = ..
Plugins = plugins
Libraries = libs
EOF

    # Recolectar librerías compartidas de Qt, ICU y SQLite usadas por el ejecutable
    echo -e "${GREEN}==> Recolectando librerías en libs/...${NC}"
    QT_LIBS=$(ldd "${BIN_PATH}" | awk '/=> \// {print $3}' | grep -E '/lib(Qt[56]|icu|sqlite3)' || true)
    for lib in ${QT_LIBS}; do
        [ -f "${lib}" ] && cp -Ln "${lib}" "${STAGE_DIR}/libs/"
    done

    # Recolectar plugins esenciales de Qt (incluyendo sqldrivers para persistencia SQLite)
    QT_PLUGINS_DIR=$("${QMAKE_BIN}" -query QT_INSTALL_PLUGINS)
    echo -e "${GREEN}==> Copiando plugins Qt desde ${QT_PLUGINS_DIR}...${NC}"
    for plugin_subdir in iconengines imageformats platforms platformthemes xcbglintegrations sqldrivers; do
        if [ -d "${QT_PLUGINS_DIR}/${plugin_subdir}" ]; then
            cp -r "${QT_PLUGINS_DIR}/${plugin_subdir}" "${STAGE_DIR}/plugins/"
        fi
    done

    # En sqldrivers, conservar el driver SQLite para evitar arrastrar dependencias de MySQL/PostgreSQL
    if [ -d "${STAGE_DIR}/plugins/sqldrivers" ]; then
        find "${STAGE_DIR}/plugins/sqldrivers" -type f -name "*.so" ! -name "*sqlite*" -delete
    fi

    # Recolectar dependencias Qt/ICU/SQLite de los propios plugins
    PLUGIN_LIBS=$(find "${STAGE_DIR}/plugins" -name "*.so" -exec ldd {} + 2>/dev/null \
        | awk '/=> \// {print $3}' \
        | grep -E '/lib(Qt[56]|icu|sqlite3)' \
        | sort -u || true)
    for plib in ${PLUGIN_LIBS}; do
        [ -f "${plib}" ] && cp -Ln "${plib}" "${STAGE_DIR}/libs/"
    done

    # Ajustar RPATH si patchelf está instalado para asegurar resolución contra ../libs
    if command -v patchelf &> /dev/null; then
        echo -e "${GREEN}==> Ajustando RPATH con patchelf...${NC}"
        patchelf --set-rpath '$ORIGIN/../libs' "${STAGE_DIR}/bin/${APP_NAME}" 2>/dev/null || true
        find "${STAGE_DIR}/libs" -type f -name "*.so*" -exec patchelf --set-rpath '$ORIGIN' {} + 2>/dev/null || true
        find "${STAGE_DIR}/plugins" -type f -name "*.so" -exec patchelf --set-rpath '$ORIGIN/../../libs' {} + 2>/dev/null || true
    else
        echo -e "${YELLOW}[Aviso] 'patchelf' no encontrado; asegúrate de definir QMAKE_LFLAGS += -Wl,-rpath,'\$\$ORIGIN/../libs' en ${APP_NAME}.pro.${NC}"
    fi

    # Eliminar símbolos innecesarios en libs/ y plugins/
    find "${STAGE_DIR}/libs" "${STAGE_DIR}/plugins" -type f -name "*.so*" -exec strip --strip-unneeded {} + 2>/dev/null || true

    # Generar entrada de menú .desktop
    generate_desktop_file "${STAGE_DIR}/${APP_NAME}.desktop"

    # Empaquetar payload tar.gz
    PAYLOAD_TAR="${BUILD_DIR}/payload_linux.tar.gz"
    tar -czf "${PAYLOAD_TAR}" -C "${STAGE_DIR}" .

    # Construir instalador autocontenido .run
    cat > "${RUN_OUTPUT}" << 'EOF'
#!/usr/bin/env bash
set -e

APP_NAME="qGlobalManager"
APP_DISPLAY_NAME="Global Manager"
INSTALL_DIR="/opt/${APP_NAME}"

if [ "$(id -u)" -ne 0 ]; then
    echo "==> Se requieren permisos de root para instalar ${APP_DISPLAY_NAME} en ${INSTALL_DIR}..."
    if command -v sudo &> /dev/null; then
        exec sudo bash "$0" "$@"
    elif command -v pkexec &> /dev/null; then
        exec pkexec bash "$0" "$@"
    else
        exec su -c "bash '$0' $*"
    fi
fi

echo "==> Instalando ${APP_DISPLAY_NAME} en ${INSTALL_DIR}..."
rm -rf "${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}"

ARCHIVE_LINE=$(awk '/^__ARCHIVE_BELOW__/ {print NR + 1; exit 0;}' "$0")
tail -n+"${ARCHIVE_LINE}" "$0" | tar -xzf - -C "${INSTALL_DIR}"

chmod 755 "${INSTALL_DIR}/bin/${APP_NAME}"
ln -sf "${INSTALL_DIR}/bin/${APP_NAME}" "/usr/local/bin/${APP_NAME}"

if [ -f "${INSTALL_DIR}/${APP_NAME}.desktop" ]; then
    mkdir -p /usr/share/applications
    cp "${INSTALL_DIR}/${APP_NAME}.desktop" "/usr/share/applications/${APP_NAME}.desktop"
fi

echo "==> ${APP_DISPLAY_NAME} instalado correctamente en ${INSTALL_DIR}."
echo "==> Puedes abrirlo desde el menú de aplicaciones o ejecutando: ${APP_NAME}"
exit 0

__ARCHIVE_BELOW__
EOF

    cat "${PAYLOAD_TAR}" >> "${RUN_OUTPUT}"
    chmod +x "${RUN_OUTPUT}"

    rm -rf "${STAGE_DIR}" "${PAYLOAD_TAR}"
    echo -e "${GREEN}==> Instalador Linux generado: ${RUN_OUTPUT} ($(du -h "${RUN_OUTPUT}" | cut -f1))${NC}"
fi

# ==============================================================================
# 3. Empaquetado autocontenido para Windows (--make-deploy-win)
# ==============================================================================
if [ "$DO_DEPLOY_WIN" = true ]; then
    WIN_QMAKE_BIN=$(detect_win_qmake)
    if [ -z "${WIN_QMAKE_BIN}" ]; then
        echo -e "${RED}[Error] No se encontró un toolchain Qt/MinGW para compilar hacia Windows.${NC}"
        echo -e "${YELLOW}Sugerencia: ejecuta '--make-deploy-win' desde MSYS2/MinGW en Windows, o configura MXE en Linux y exporta WIN_QMAKE.${NC}"
        exit 1
    fi

    WIN_BUILD_DIR="${BUILD_DIR}/win_release"
    compile_project "${WIN_QMAKE_BIN}" "${WIN_BUILD_DIR}" "release" "${WIN_INSTALL_DIR}"

    WIN_BIN_PATH=""
    for candidate in "${WIN_BUILD_DIR}/${APP_NAME}.exe" "${WIN_BUILD_DIR}/release/${APP_NAME}.exe"; do
        if [ -f "${candidate}" ]; then
            WIN_BIN_PATH="${candidate}"
            break
        fi
    done

    if [ -z "${WIN_BIN_PATH}" ]; then
        echo -e "${RED}[Error] No se encontró '${APP_NAME}.exe' tras la compilación para Windows.${NC}"
        exit 1
    fi

    WIN_ARCH="x86_64"
    case "${WIN_QMAKE_BIN}" in
        *i686*|*mingw32*) WIN_ARCH="x86" ;;
    esac

    STAGE_WIN_DIR="${BUILD_DIR}/deploy_stage_win"
    mkdir -p "${DIST_WIN_DIR}"
    EXE_OUTPUT="${DIST_WIN_DIR}/${APP_NAME}-${WIN_ARCH}.exe"

    echo -e "${CYAN}==> Preparando paquete autocontenido Windows (${WIN_ARCH})...${NC}"
    rm -rf "${STAGE_WIN_DIR}"
    mkdir -p "${STAGE_WIN_DIR}/bin" "${STAGE_WIN_DIR}/libs" "${STAGE_WIN_DIR}/plugins"

    cp "${WIN_BIN_PATH}" "${STAGE_WIN_DIR}/bin/${APP_NAME}.exe"
    if command -v x86_64-w64-mingw32-strip &> /dev/null; then
        x86_64-w64-mingw32-strip --strip-unneeded "${STAGE_WIN_DIR}/bin/${APP_NAME}.exe" 2>/dev/null || true
    elif [ "$IS_WINDOWS_HOST" = true ] && command -v strip &> /dev/null; then
        strip --strip-unneeded "${STAGE_WIN_DIR}/bin/${APP_NAME}.exe" 2>/dev/null || true
    fi

    copy_project_assets "${STAGE_WIN_DIR}"

    # Generar qt.conf para resolución de plugins y librerías
    cat > "${STAGE_WIN_DIR}/bin/qt.conf" << EOF
[Paths]
Prefix = ..
Plugins = plugins
Libraries = libs
EOF

    # Recolectar DLLs y plugins usando windeployqt si existe (entorno Windows) o desde Qt/MinGW (cross-compile)
    WINDEPLOYQT_BIN=""
    if command -v windeployqt6 &> /dev/null; then
        WINDEPLOYQT_BIN="windeployqt6"
    elif command -v windeployqt &> /dev/null; then
        WINDEPLOYQT_BIN="windeployqt"
    fi

    if [ "$IS_WINDOWS_HOST" = true ] && [ -n "${WINDEPLOYQT_BIN}" ]; then
        echo -e "${GREEN}==> Recolectando dependencias con ${WINDEPLOYQT_BIN}...${NC}"
        TMP_WDEPLOY="${BUILD_DIR}/tmp_windeploy"
        rm -rf "${TMP_WDEPLOY}"
        mkdir -p "${TMP_WDEPLOY}"
        cp "${STAGE_WIN_DIR}/bin/${APP_NAME}.exe" "${TMP_WDEPLOY}/"

        "${WINDEPLOYQT_BIN}" --release --no-translations --no-compiler-runtime "${TMP_WDEPLOY}/${APP_NAME}.exe"

        # Mover DLLs a libs/ y subcarpetas de plugins a plugins/
        find "${TMP_WDEPLOY}" -maxdepth 1 -type f -name "*.dll" -exec cp {} "${STAGE_WIN_DIR}/libs/" \;
        for pdir in iconengines imageformats platforms sqldrivers styles tls; do
            if [ -d "${TMP_WDEPLOY}/${pdir}" ]; then
                cp -r "${TMP_WDEPLOY}/${pdir}" "${STAGE_WIN_DIR}/plugins/"
            fi
        done

        # Copiar runtimes básicos de MinGW si están en el directorio de qmake
        QT_BIN_DIR=$("${WIN_QMAKE_BIN}" -query QT_INSTALL_BINS)
        for rt_dll in libgcc_s_seh-1.dll libgcc_s_dw2-1.dll libstdc++-6.dll libwinpthread-1.dll; do
            [ -f "${QT_BIN_DIR}/${rt_dll}" ] && cp "${QT_BIN_DIR}/${rt_dll}" "${STAGE_WIN_DIR}/libs/"
        done

        rm -rf "${TMP_WDEPLOY}"
    else
        echo -e "${GREEN}==> Recolectando DLLs y plugins Qt desde el sysroot de MinGW...${NC}"
        QT_BIN_DIR=$("${WIN_QMAKE_BIN}" -query QT_INSTALL_BINS)
        QT_PLUGINS_DIR=$("${WIN_QMAKE_BIN}" -query QT_INSTALL_PLUGINS)

        for plugin_subdir in iconengines imageformats platforms sqldrivers styles; do
            if [ -d "${QT_PLUGINS_DIR}/${plugin_subdir}" ]; then
                cp -r "${QT_PLUGINS_DIR}/${plugin_subdir}" "${STAGE_WIN_DIR}/plugins/"
            fi
        done

        if [ -d "${STAGE_WIN_DIR}/plugins/sqldrivers" ]; then
            find "${STAGE_WIN_DIR}/plugins/sqldrivers" -type f -name "*.dll" ! -name "*sqlite*" -delete
        fi

        OBJDUMP_BIN=$(command -v x86_64-w64-mingw32-objdump || command -v objdump || echo "")
        if [ -n "${OBJDUMP_BIN}" ]; then
            NEEDED_DLLS=$(find "${STAGE_WIN_DIR}/bin" "${STAGE_WIN_DIR}/plugins" -type f \( -name "*.exe" -o -name "*.dll" \) \
                -exec "${OBJDUMP_BIN}" -p {} + 2>/dev/null \
                | awk '/DLL Name:/ {print $3}' | sort -u)
            for dll in ${NEEDED_DLLS}; do
                [ -f "${QT_BIN_DIR}/${dll}" ] && cp -n "${QT_BIN_DIR}/${dll}" "${STAGE_WIN_DIR}/libs/"
            done
        fi
    fi

    # Generar script NSIS para empaquetar el instalador .exe
    NSI_FILE="${BUILD_DIR}/${APP_NAME}.nsi"
    ABS_EXE_OUTPUT="$(realpath -m "${EXE_OUTPUT}" 2>/dev/null || echo "$(pwd)/${EXE_OUTPUT}")"
    ABS_STAGE_WIN="$(realpath "${STAGE_WIN_DIR}" 2>/dev/null || echo "$(pwd)/${STAGE_WIN_DIR}")"

    cat > "${NSI_FILE}" << EOF
!define APP_NAME "${APP_NAME}"
!define APP_DISPLAY_NAME "${APP_DISPLAY_NAME}"
!define APP_VERSION "${APP_VERSION}"

Name "\${APP_DISPLAY_NAME} \${APP_VERSION}"
OutFile "${ABS_EXE_OUTPUT}"
InstallDir "${WIN_INSTALL_DIR}"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

Page directory
Page instfiles
UninstPage confirm
UninstPage instfiles

Section "Instalar \${APP_DISPLAY_NAME}"
    SetOutPath "\$INSTDIR"
    File /r "${ABS_STAGE_WIN}\\*.*"

    ; Registrar App Paths para que bin\\${APP_NAME}.exe resuelva DLLs en libs\\
    WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\${APP_NAME}.exe" "" "\$INSTDIR\\bin\\${APP_NAME}.exe"
    WriteRegStr HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\${APP_NAME}.exe" "Path" "\$INSTDIR\\bin;\$INSTDIR\\libs"

    ; Accesos directos en Menú Inicio
    CreateDirectory "\$SMPROGRAMS\\\${APP_DISPLAY_NAME}"
    CreateShortCut "\$SMPROGRAMS\\\${APP_DISPLAY_NAME}\\\${APP_DISPLAY_NAME}.lnk" "\$INSTDIR\\bin\\${APP_NAME}.exe"
    CreateShortCut "\$SMPROGRAMS\\\${APP_DISPLAY_NAME}\\Desinstalar \${APP_DISPLAY_NAME}.lnk" "\$INSTDIR\\uninstall.exe"

    WriteUninstaller "\$INSTDIR\\uninstall.exe"
SectionEnd

Section "Uninstall"
    DeleteRegKey HKLM "Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\${APP_NAME}.exe"
    Delete "\$SMPROGRAMS\\\${APP_DISPLAY_NAME}\\\${APP_DISPLAY_NAME}.lnk"
    Delete "\$SMPROGRAMS\\\${APP_DISPLAY_NAME}\\Desinstalar \${APP_DISPLAY_NAME}.lnk"
    RMDir "\$SMPROGRAMS\\\${APP_DISPLAY_NAME}"
    RMDir /r "\$INSTDIR"
SectionEnd
EOF

    if command -v makensis &> /dev/null; then
        echo -e "${GREEN}==> Generando instalador Windows (.exe) con NSIS...${NC}"
        makensis -V2 "${NSI_FILE}"
        rm -rf "${STAGE_WIN_DIR}" "${NSI_FILE}"
        echo -e "${GREEN}==> Instalador Windows generado: ${EXE_OUTPUT} ($(du -h "${EXE_OUTPUT}" | cut -f1))${NC}"
    else
        echo -e "${YELLOW}[Aviso] 'makensis' (NSIS) no está instalado. No se pudo empaquetar '${EXE_OUTPUT}'.${NC}"
        echo -e "${YELLOW}==> Se dejó la estructura lista en '${STAGE_WIN_DIR}' y el script en '${NSI_FILE}'.${NC}"
        echo -e "${YELLOW}==> Instala NSIS y ejecuta: makensis '${NSI_FILE}'${NC}"
    fi
fi

# ==============================================================================
# 4. Compilación estándar de desarrollo/release (cuando no se pidió deploy)
# ==============================================================================
if [ "$DO_DEPLOY_LINUX" = false ] && [ "$DO_DEPLOY_WIN" = false ]; then
    QMAKE_BIN=$(detect_native_qmake)
    if [ -z "${QMAKE_BIN}" ]; then
        echo -e "${RED}[Error] No se encontró qmake en el sistema.${NC}"
        exit 1
    fi

    compile_project "${QMAKE_BIN}" "${BUILD_DIR}" "${BUILD_MODE}" "${LINUX_INSTALL_DIR}"

    if [ "$IS_WINDOWS_HOST" = true ]; then
        BIN_PATH="${BUILD_DIR}/${APP_NAME}.exe"
        [ ! -f "${BIN_PATH}" ] && [ -f "${BUILD_DIR}/${BUILD_MODE}/${APP_NAME}.exe" ] && BIN_PATH="${BUILD_DIR}/${BUILD_MODE}/${APP_NAME}.exe"
    else
        BIN_PATH="${BUILD_DIR}/${APP_NAME}"
    fi

    echo -e "${GREEN}==> Compilación exitosa: ${BIN_PATH}${NC}"

    # 5. Instalación directa en /opt/qGlobalManager/ (Linux)
    if [ "$DO_INSTALL" = true ]; then
        if [ "$IS_WINDOWS_HOST" = true ]; then
            echo -e "${YELLOW}[Aviso] En Windows utiliza --make-deploy-win para generar el instalador en '${DIST_WIN_DIR}/'.${NC}"
        else
            echo -e "${YELLOW}==> Instalando ${APP_DISPLAY_NAME} en ${LINUX_INSTALL_DIR}/...${NC}"
            TMP_DESKTOP="${BUILD_DIR}/${APP_NAME}.desktop"
            generate_desktop_file "${TMP_DESKTOP}"

            INSTALL_CMD="mkdir -p '${LINUX_INSTALL_DIR}/bin' '${LINUX_INSTALL_DIR}/icons' '${LINUX_INSTALL_DIR}/documentation' && \
                         cp '${BIN_PATH}' '${LINUX_INSTALL_DIR}/bin/${APP_NAME}' && \
                         chmod 755 '${LINUX_INSTALL_DIR}/bin/${APP_NAME}' && \
                         ln -sf '${LINUX_INSTALL_DIR}/bin/${APP_NAME}' '/usr/local/bin/${APP_NAME}' && \
                         cp '${TMP_DESKTOP}' '/usr/share/applications/${APP_NAME}.desktop'"

            if [ -d "resources/icons" ] && [ "$(ls -A resources/icons 2>/dev/null)" ]; then
                INSTALL_CMD="${INSTALL_CMD} && cp -r resources/icons/. '${LINUX_INSTALL_DIR}/icons/'"
            elif [ -d "icons" ] && [ "$(ls -A icons 2>/dev/null)" ]; then
                INSTALL_CMD="${INSTALL_CMD} && cp -r icons/. '${LINUX_INSTALL_DIR}/icons/'"
            fi

            if [ -d "documentation" ] && [ "$(ls -A documentation 2>/dev/null)" ]; then
                INSTALL_CMD="${INSTALL_CMD} && cp -r documentation/. '${LINUX_INSTALL_DIR}/documentation/'"
            fi

            if [ "$(id -u)" -eq 0 ]; then
                eval "${INSTALL_CMD}"
            elif command -v sudo &> /dev/null; then
                sudo bash -c "${INSTALL_CMD}"
            elif command -v pkexec &> /dev/null; then
                pkexec bash -c "${INSTALL_CMD}"
            else
                echo -e "${YELLOW}Introduce la contraseña de root para instalar en ${LINUX_INSTALL_DIR}:${NC}"
                su -c "${INSTALL_CMD}"
            fi

            rm -f "${TMP_DESKTOP}"
            echo -e "${GREEN}==> Instalación completada en ${LINUX_INSTALL_DIR}/bin/${APP_NAME}${NC}"
        fi
    fi

    # 6. Ejecución si se indicó --run o --root
    if [ "$DO_RUN" = true ]; then
        if [ "$DO_INSTALL" = true ] && [ "$IS_WINDOWS_HOST" = false ]; then
            EXEC_TARGET="${LINUX_INSTALL_DIR}/bin/${APP_NAME}"
        else
            EXEC_TARGET="./${BIN_PATH}"
        fi

        if [ "$AS_ROOT" = true ] && [ "$IS_WINDOWS_HOST" = false ]; then
            echo -e "${YELLOW}==> Ejecutando ${EXEC_TARGET} como root...${NC}"
            if [ "$(id -u)" -eq 0 ]; then
                "${EXEC_TARGET}"
            elif command -v pkexec &> /dev/null; then
                pkexec env DISPLAY="$DISPLAY" XAUTHORITY="$XAUTHORITY" "$(realpath "${EXEC_TARGET}")"
            elif command -v sudo &> /dev/null; then
                sudo -E "${EXEC_TARGET}"
            else
                su -c "${EXEC_TARGET}"
            fi
        else
            echo -e "${GREEN}==> Ejecutando ${EXEC_TARGET}...${NC}"
            "${EXEC_TARGET}"
        fi
    fi
fi
