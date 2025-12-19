#!/bin/bash

# FlameGraph generator script for C++ programs on Arch Linux
# Usage: ./performance.sh <program_path> [output_svg] -- <program_args>

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
DEFAULT_SVG="flamegraph.svg"
FLAMEGRAPH_DIR="$HOME/Desktop/FlameGraph"  # Default FlameGraph location

# Function to print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1"
}

# Function to show usage
show_usage() {
    echo "Usage: $0 <program_path> [output_svg] -- [program_args...]"
    echo ""
    echo "Examples:"
    echo "  $0 ./my_program"
    echo "  $0 ./my_program output.svg"
    echo "  $0 ./my_program output.svg -- arg1 arg2 arg3"
    echo "  $0 ./N1 perf_graph.svg -- c lena2.bmp lena.test"
    echo ""
    echo "Note: Use '--' to separate script arguments from program arguments"
}

# Check if at least one argument is provided
if [ $# -lt 1 ]; then
    show_usage
    exit 1
fi

# Parse arguments
PROGRAM_PATH="$1"
OUTPUT_SVG="${2:-$DEFAULT_SVG}"

# Check if third argument is '--'
if [ "$3" = "--" ]; then
    # Collect all remaining arguments as program arguments
    shift 3
    PROGRAM_ARGS=("$@")
else
    # No program arguments provided
    PROGRAM_ARGS=()
fi

# Check if program exists
if [ ! -f "$PROGRAM_PATH" ]; then
    print_error "Program '$PROGRAM_PATH' not found!"
    exit 1
fi

# Check if program is executable
if [ ! -x "$PROGRAM_PATH" ]; then
    print_warn "Program '$PROGRAM_PATH' is not executable. Attempting to make it executable..."
    chmod +x "$PROGRAM_PATH" 2>/dev/null || {
        print_error "Failed to make program executable. Please check permissions."
        exit 1
    }
fi

# Check if FlameGraph directory exists
if [ ! -d "$FLAMEGRAPH_DIR" ]; then
    print_error "FlameGraph directory not found at $FLAMEGRAPH_DIR"
    print_info "Please install it with:"
    echo "  git clone https://github.com/brendangregg/FlameGraph.git $FLAMEGRAPH_DIR"
    exit 1
fi

# Check if required FlameGraph scripts exist
STACKCOLLAPSE="$FLAMEGRAPH_DIR/stackcollapse-perf.pl"
FLAMEGRAPH="$FLAMEGRAPH_DIR/flamegraph.pl"

if [ ! -f "$STACKCOLLAPSE" ]; then
    print_error "stackcollapse-perf.pl not found in $FLAMEGRAPH_DIR"
    exit 1
fi

if [ ! -f "$FLAMEGRAPH" ]; then
    print_error "flamegraph.pl not found in $FLAMEGRAPH_DIR"
    exit 1
fi

# Check if perf is available
if ! command -v perf &> /dev/null; then
    print_error "'perf' is not installed. Please install it first:"
    echo "  sudo pacman -S perf"
    exit 1
fi

# Check perf permissions
check_perf_permissions() {
    # Check if perf can run without sudo
    if ! perf record --help &> /dev/null; then
        print_warn "perf requires elevated permissions."
        return 1
    fi
    return 0
}

# Function to clean up temporary files
cleanup() {
    if [ -f "perf.data" ]; then
        rm -f perf.data
        print_info "Cleaned up perf.data"
    fi
    if [ -f "out.folded" ]; then
        rm -f out.folded
        print_info "Cleaned up out.folded"
    fi
    if [ -f "perf.log" ]; then
        rm -f perf.log
        print_info "Cleaned up perf.log"
    fi
}

# Main execution
main() {
    print_info "Starting FlameGraph generation..."
    print_info "Program: $PROGRAM_PATH"
    if [ ${#PROGRAM_ARGS[@]} -gt 0 ]; then
        print_info "Program arguments: ${PROGRAM_ARGS[*]}"
    fi
    print_info "Output: $OUTPUT_SVG"
    
    # Set trap to clean up on exit
    trap cleanup EXIT INT TERM
    
    # Check perf permissions
    if ! check_perf_permissions; then
        print_info "Trying with sudo..."
        PERF_CMD="sudo perf"
    else
        PERF_CMD="perf"
    fi
    
    # Step 1: Test run the program first
    print_info "Testing program execution..."
    if [ ${#PROGRAM_ARGS[@]} -gt 0 ]; then
        print_debug "Running: $PROGRAM_PATH ${PROGRAM_ARGS[*]}"
        if ! "$PROGRAM_PATH" "${PROGRAM_ARGS[@]}" >/dev/null 2>&1; then
            print_warn "Program test run failed. Running with output visible..."
            echo "=== PROGRAM OUTPUT ==="
            "$PROGRAM_PATH" "${PROGRAM_ARGS[@]}"
            echo "=== END PROGRAM OUTPUT ==="
            print_warn "Continuing anyway..."
        else
            print_info "Program test run successful"
        fi
    fi
    
    # Step 2: Profile the program with perf
    print_info "Profiling with perf..."
    print_debug "Command: $PERF_CMD record -F 99 -g --output=perf.data -- $PROGRAM_PATH ${PROGRAM_ARGS[*]}"
    
    # Create a simple wrapper script for perf to ensure proper argument handling
    cat > /tmp/perf_wrapper.sh << EOF
#!/bin/bash
exec $PERF_CMD record -F 99 -g --output=perf.data -- "$PROGRAM_PATH" "${PROGRAM_ARGS[@]}"
EOF
    chmod +x /tmp/perf_wrapper.sh
    
    # Run perf
    if /tmp/perf_wrapper.sh 2>&1 | tee perf.log; then
        print_info "perf record completed"
    else
        print_error "perf record failed. Check perf.log for details"
        
        # Check if perf.data was created
        if [ ! -f "perf.data" ]; then
            print_error "perf.data was not created. Common issues:"
            echo "  1. perf permissions: try running script with sudo"
            echo "  2. Program exits too quickly: ensure it runs for at least a few seconds"
            echo "  3. Adjust kernel.perf_event_paranoid:"
            echo "     sudo sysctl -w kernel.perf_event_paranoid=1"
            exit 1
        fi
    fi
    
    # Check if perf.data exists and has content
    if [ ! -f "perf.data" ]; then
        print_error "perf.data file was not created!"
        print_info "Try running perf manually:"
        echo "  $PERF_CMD record -F 99 -g ./N1 c lena2.bmp lena.test"
        exit 1
    fi
    
    DATA_SIZE=$(stat -c%s perf.data 2>/dev/null || echo "0")
    if [ "$DATA_SIZE" -lt 1000 ]; then
        print_warn "perf.data is very small ($DATA_SIZE bytes). The program might have exited too quickly."
    fi
    
    # Step 3: Convert perf data
    print_info "Converting perf data..."
    if ! $PERF_CMD script > perf.script 2>&1; then
        print_error "Failed to run 'perf script'"
        print_info "Trying with sudo..."
        sudo perf script > perf.script 2>&1 || {
            print_error "Even sudo failed. Check perf.data file"
            exit 1
        }
    fi
    
    if [ ! -s perf.script ]; then
        print_error "perf script output is empty"
        exit 1
    fi
    
    if ! "$STACKCOLLAPSE" perf.script > out.folded 2>&1; then
        print_error "Failed to run stackcollapse-perf.pl"
        print_info "Check if perf.script contains data:"
        head -20 perf.script
        exit 1
    fi
    
    # Step 4: Generate flame graph
    print_info "Generating flame graph..."
    FOLDED_SIZE=$(stat -c%s out.folded 2>/dev/null || echo "0")
    if [ "$FOLDED_SIZE" -lt 10 ]; then
        print_error "out.folded is too small ($FOLDED_SIZE bytes). No stack data captured."
        print_info "Possible reasons:"
        echo "  1. Program ran too briefly"
        echo "  2. No CPU samples were collected"
        echo "  3. perf configuration issues"
        print_info "Try running with longer program execution or different sampling frequency"
        exit 1
    fi
    
    if ! "$FLAMEGRAPH" out.folded > "$OUTPUT_SVG" 2>&1; then
        print_error "Failed to generate flame graph"
        print_info "Check out.folded content:"
        head -20 out.folded
        exit 1
    fi
    
    # Step 5: Check if file was created
    if [ -f "$OUTPUT_SVG" ]; then
        print_info "Flame graph successfully created: $OUTPUT_SVG"
        print_info "File size: $(du -h "$OUTPUT_SVG" | cut -f1)"
        
        # Offer to open the file
        read -p "Open flame graph in browser? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            if command -v xdg-open &> /dev/null; then
                xdg-open "$OUTPUT_SVG"
            elif command -v firefox &> /dev/null; then
                firefox "$OUTPUT_SVG"
            else
                print_warn "Could not find a browser to open the file."
                print_info "You can open it manually with: firefox $OUTPUT_SVG"
            fi
        fi
    else
        print_error "Flame graph file was not created."
        exit 1
    fi
}

# Run the main function
main