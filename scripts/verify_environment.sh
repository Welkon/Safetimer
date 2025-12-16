#!/bin/sh
# SafeTimer Environment Verification Script
# Checks development environment and provides actionable installation suggestions
# POSIX-compliant (works on Linux, macOS, WSL)
# Last updated: 2025-12-13

# Version requirements
GCC_MIN_VERSION="4.8"
SDCC_MIN_VERSION="4.2.0"
UNITY_RECOMMENDED="2.5.2"

# Exit codes
EXIT_SUCCESS=0
EXIT_CRITICAL=1
EXIT_HOST_TEST=2
EXIT_WARNING=3

# Color codes (if terminal supports)
if [ -t 1 ]; then
    GREEN='\033[0;32m'
    RED='\033[0;31m'
    YELLOW='\033[1;33m'
    BLUE='\033[0;34m'
    NC='\033[0m' # No Color
else
    GREEN=''
    RED=''
    YELLOW=''
    BLUE=''
    NC=''
fi

# Global counters
CRITICAL_OK=0
CRITICAL_TOTAL=0
HOST_OK=0
HOST_TOTAL=0
EMBEDDED_OK=0
EMBEDDED_TOTAL=0

# Mode flags
CHECK_ALL=1
CHECK_HOST=0
CHECK_8BIT=0
JSON_OUTPUT=0

# ========== Helper Functions ==========

print_header() {
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo "=== SafeTimer Environment Verification Report ==="
        echo "Generated: $(date '+%Y-%m-%d %H:%M:%S')"
        echo ""
    fi
}

print_section() {
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo ""
        echo "$1"
    fi
}

# Version comparison (returns 0 if ver1 >= ver2)
version_gte() {
    # Extract version numbers
    ver1=$(echo "$1" | grep -oE '[0-9]+(\.[0-9]+)*' | head -1)
    ver2="$2"

    # Compare using awk
    result=$(awk -v v1="$ver1" -v v2="$ver2" '
        BEGIN {
            split(v1, a, ".");
            split(v2, b, ".");
            for (i=1; i<=3; i++) {
                if (a[i] < b[i]) { print 0; exit }
                if (a[i] > b[i]) { print 1; exit }
            }
            print 1
        }
    ')
    [ "$result" -eq 1 ]
}

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Detect platform
detect_platform() {
    if [ -f /proc/version ] && grep -qi microsoft /proc/version; then
        echo "WSL"
    elif [ "$(uname -s)" = "Darwin" ]; then
        echo "macOS"
    elif [ "$(uname -s)" = "Linux" ]; then
        if [ -f /etc/os-release ]; then
            . /etc/os-release
            echo "Linux-$ID"
        else
            echo "Linux"
        fi
    else
        echo "Unknown"
    fi
}

# Get install command for platform
get_install_cmd() {
    tool="$1"
    platform="$2"

    case "$platform" in
        WSL|Linux-ubuntu|Linux-debian)
            echo "sudo apt-get install $tool"
            ;;
        macOS)
            case "$tool" in
                gcc) echo "brew install gcc" ;;
                sdcc) echo "brew install sdcc" ;;
                cppcheck) echo "brew install cppcheck" ;;
                gcov) echo "gcov is included with gcc" ;;
                *) echo "brew install $tool" ;;
            esac
            ;;
        Linux-fedora|Linux-rhel|Linux-centos)
            echo "sudo yum install $tool"
            ;;
        Linux-arch)
            echo "sudo pacman -S $tool"
            ;;
        *)
            echo "Install using your system package manager"
            ;;
    esac
}

# ========== Tool Checking Functions ==========

check_gcc() {
    CRITICAL_TOTAL=$((CRITICAL_TOTAL + 1))

    if command_exists gcc; then
        ver=$(gcc --version | head -1 || echo "unknown")

        # Check if it's actually Clang masquerading as gcc (macOS)
        if echo "$ver" | grep -qi "Apple clang\|clang"; then
            CRITICAL_OK=$((CRITICAL_OK + 1))
            if [ "$JSON_OUTPUT" -eq 0 ]; then
                printf "${GREEN}✅${NC} %-12s: %s (Apple Clang, C99 compatible)\n" "gcc/clang" "$ver"
            fi
            return 0
        fi

        # Real GCC - check version
        if version_gte "$ver" "$GCC_MIN_VERSION"; then
            CRITICAL_OK=$((CRITICAL_OK + 1))
            if [ "$JSON_OUTPUT" -eq 0 ]; then
                printf "${GREEN}✅${NC} %-12s: %s (>= %s required)\n" "gcc" "$ver" "$GCC_MIN_VERSION"
            fi
            return 0
        else
            if [ "$JSON_OUTPUT" -eq 0 ]; then
                printf "${RED}❌${NC} %-12s: %s (< %s required)\n" "gcc" "$ver" "$GCC_MIN_VERSION"
                echo "   → Upgrade: $(get_install_cmd gcc "$PLATFORM")"
            fi
            return 1
        fi
    else
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${RED}❌${NC} %-12s: NOT FOUND\n" "gcc"
            echo "   → Install: $(get_install_cmd gcc "$PLATFORM")"
        fi
        return 1
    fi
}

check_make() {
    CRITICAL_TOTAL=$((CRITICAL_TOTAL + 1))

    if command_exists make; then
        ver=$(make --version | head -1 || echo "unknown")
        CRITICAL_OK=$((CRITICAL_OK + 1))
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${GREEN}✅${NC} %-12s: %s\n" "make" "$ver"
        fi
        return 0
    else
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${RED}❌${NC} %-12s: NOT FOUND\n" "make"
            echo "   → Install: $(get_install_cmd make "$PLATFORM")"
        fi
        return 1
    fi
}

check_gcov() {
    HOST_TOTAL=$((HOST_TOTAL + 1))

    if command_exists gcov; then
        ver=$(gcov --version | head -1 || echo "unknown")
        HOST_OK=$((HOST_OK + 1))
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${GREEN}✅${NC} %-12s: %s\n" "gcov" "$ver"
        fi
        return 0
    else
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${YELLOW}❌${NC} %-12s: NOT FOUND\n" "gcov"
            echo "   → Install: $(get_install_cmd gcc "$PLATFORM") (gcov included)"
        fi
        return 1
    fi
}

check_cppcheck() {
    HOST_TOTAL=$((HOST_TOTAL + 1))

    if command_exists cppcheck; then
        ver=$(cppcheck --version 2>&1 | grep -oE '[0-9]+\.[0-9]+' | head -1 || echo "unknown")
        HOST_OK=$((HOST_OK + 1))
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${GREEN}✅${NC} %-12s: %s\n" "cppcheck" "$ver"
        fi
        return 0
    else
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${YELLOW}❌${NC} %-12s: NOT FOUND\n" "cppcheck"
            echo "   → Install: $(get_install_cmd cppcheck "$PLATFORM")"
            echo "   → Note: Optional for static analysis (make static-analysis)"
        fi
        return 1
    fi
}

check_sdcc() {
    EMBEDDED_TOTAL=$((EMBEDDED_TOTAL + 1))

    if command_exists sdcc; then
        ver=$(sdcc --version 2>&1 | head -1 || echo "unknown")

        if version_gte "$ver" "$SDCC_MIN_VERSION"; then
            EMBEDDED_OK=$((EMBEDDED_OK + 1))
            if [ "$JSON_OUTPUT" -eq 0 ]; then
                printf "${GREEN}✅${NC} %-12s: %s (>= %s required)\n" "sdcc" "$ver" "$SDCC_MIN_VERSION"
            fi
            return 0
        else
            if [ "$JSON_OUTPUT" -eq 0 ]; then
                printf "${RED}❌${NC} %-12s: %s (< %s required)\n" "sdcc" "$ver" "$SDCC_MIN_VERSION"
                echo "   → Upgrade: $(get_install_cmd sdcc "$PLATFORM")"
            fi
            return 1
        fi
    else
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${YELLOW}❌${NC} %-12s: NOT FOUND\n" "sdcc"
            echo "   → Install: $(get_install_cmd sdcc "$PLATFORM")"
            echo "   → Note: Only required for 8-bit MCU development (Story A.1-A.3)"
        fi
        return 1
    fi
}

check_unity() {
    UNITY_DIR="test/unity"

    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo ""
        echo "[UNITY FRAMEWORK]"
    fi

    if [ -f "$UNITY_DIR/unity.c" ]; then
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${GREEN}✅${NC} %-30s: INSTALLED\n" "$UNITY_DIR/unity.c"
        fi
        return 0
    else
        if [ "$JSON_OUTPUT" -eq 0 ]; then
            printf "${YELLOW}❌${NC} %-30s: NOT FOUND\n" "$UNITY_DIR/unity.c"
            echo "   → Run: make unity"
            echo "   → Or manually download from throwtheswitch.org/unity"
        fi
        return 1
    fi
}

check_optional_tools() {
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo ""
        echo "[OPTIONAL TOOLS]"

        # wget or curl (for make unity)
        if command_exists wget; then
            printf "${GREEN}✅${NC} %-12s: %s\n" "wget" "$(wget --version | head -1)"
        elif command_exists curl; then
            printf "${GREEN}✅${NC} %-12s: %s\n" "curl" "$(curl --version | head -1)"
        else
            printf "${YELLOW}⚠️${NC}  %-12s: Neither wget nor curl found\n" "download"
            echo "   → Install one for 'make unity' to work"
            echo "   → $(get_install_cmd wget "$PLATFORM")"
        fi

        # python3 (for future scripts)
        if command_exists python3; then
            printf "${GREEN}✅${NC} %-12s: %s\n" "python3" "$(python3 --version)"
        else
            printf "${BLUE}○${NC}  %-12s: NOT FOUND (optional)\n" "python3"
        fi
    fi
}

# ========== Main Execution ==========

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --all)
            CHECK_ALL=1
            shift
            ;;
        --host)
            CHECK_ALL=0
            CHECK_HOST=1
            shift
            ;;
        --8bit)
            CHECK_ALL=0
            CHECK_8BIT=1
            shift
            ;;
        --json)
            JSON_OUTPUT=1
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --all       Check all tools (default)"
            echo "  --host      Check only host development tools"
            echo "  --8bit      Check only 8-bit MCU tools"
            echo "  --json      Output in JSON format"
            echo "  --help      Show this help message"
            echo ""
            echo "Exit codes:"
            echo "  0 - All required tools OK"
            echo "  1 - Critical tools missing (gcc, make)"
            echo "  2 - Host testing tools missing (gcov, cppcheck)"
            echo "  3 - Only warnings (embedded tools, optional tools)"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Run '$0 --help' for usage information"
            exit 1
            ;;
    esac
done

# Detect platform
PLATFORM=$(detect_platform)

# Print header
if [ "$JSON_OUTPUT" -eq 0 ]; then
    print_header

    echo "[SYSTEM INFO]"
    echo "OS: $PLATFORM"
    echo "Shell: $SHELL"
fi

# Run checks
if [ "$JSON_OUTPUT" -eq 0 ]; then
    print_section "[CRITICAL TOOLS]"
fi
check_gcc
gcc_result=$?
check_make
make_result=$?

if [ "$CHECK_ALL" -eq 1 ] || [ "$CHECK_HOST" -eq 1 ]; then
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        print_section "[HOST TESTING TOOLS]"
    fi
    check_gcov
    check_cppcheck
fi

if [ "$CHECK_ALL" -eq 1 ] || [ "$CHECK_8BIT" -eq 1 ]; then
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        print_section "[EMBEDDED TOOLS (8-bit MCU)]"
    fi
    check_sdcc
fi

# Check Unity
check_unity
unity_result=$?

# Check optional tools
if [ "$CHECK_ALL" -eq 1 ]; then
    check_optional_tools
fi

# Print summary
if [ "$JSON_OUTPUT" -eq 0 ]; then
    echo ""
    echo "[SUMMARY]"
    printf "Critical Tools:   %d/%d OK\n" "$CRITICAL_OK" "$CRITICAL_TOTAL"

    if [ "$CHECK_ALL" -eq 1 ] || [ "$CHECK_HOST" -eq 1 ]; then
        printf "Host Testing:     %d/%d OK\n" "$HOST_OK" "$HOST_TOTAL"
    fi

    if [ "$CHECK_ALL" -eq 1 ] || [ "$CHECK_8BIT" -eq 1 ]; then
        printf "Embedded Tools:   %d/%d OK\n" "$EMBEDDED_OK" "$EMBEDDED_TOTAL"
    fi

    echo ""
    echo "[VERDICT]"
fi

# Determine exit code
if [ "$CRITICAL_OK" -lt "$CRITICAL_TOTAL" ]; then
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo "${RED}🔴 CRITICAL ISSUES DETECTED${NC} - Cannot proceed with builds"
        echo "→ Fix critical tools first, then re-run this script"
    fi
    exit $EXIT_CRITICAL
elif [ "$CHECK_HOST" -eq 1 ] && [ "$HOST_OK" -lt "$HOST_TOTAL" ]; then
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo "${YELLOW}🟡 HOST TESTING INCOMPLETE${NC} - Some test tools missing"
        echo "→ Install missing tools for full testing capability"
    fi
    exit $EXIT_HOST_TEST
elif [ "$unity_result" -ne 0 ]; then
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo "${YELLOW}🟡 UNITY NOT INSTALLED${NC} - Tests cannot run"
        echo "→ Run 'make unity' to install Unity framework"
    fi
    exit $EXIT_WARNING
else
    if [ "$JSON_OUTPUT" -eq 0 ]; then
        echo "${GREEN}✅ ALL CHECKS PASSED${NC} - Ready for development!"
        echo ""
        echo "Next steps:"
        echo "  1. Run 'make unity' if Unity not installed"
        echo "  2. Run 'make test' to verify everything works"
        echo "  3. See docs/getting_started.md for detailed guide"
    fi
    exit $EXIT_SUCCESS
fi
