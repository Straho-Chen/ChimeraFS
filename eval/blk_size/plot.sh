#!/bin/bash

# Define variables
CONDA_ENV="base"  # Replace with your Conda environment name
NOTEBOOK_FILE="plot.ipynb"
OUTPUT_NOTEBOOK="executed_plot.ipynb"

# Activate Conda environment
source "$(conda info --base)/etc/profile.d/conda.sh" 2>/dev/null || {
    echo "Error: Failed to load Conda configuration"
    exit 1
}

conda activate "$CONDA_ENV" || {
    echo "Error: Failed to activate Conda environment '$CONDA_ENV'"
    exit 1
}

# Check if notebook file exists
if [[ ! -f "$NOTEBOOK_FILE" ]]; then
    echo "Error: Notebook file '$NOTEBOOK_FILE' not found"
    exit 1
fi

# Execute notebook
echo "Executing notebook $NOTEBOOK_FILE..."
jupyter nbconvert --to notebook --execute "$NOTEBOOK_FILE" --output "$OUTPUT_NOTEBOOK" || {
    echo "Error: Notebook execution failed"
    exit 1
}

echo "Notebook execution completed! Figure saved as: *.pdf"