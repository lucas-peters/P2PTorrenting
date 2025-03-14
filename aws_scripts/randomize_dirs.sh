#!/bin/bash
# This script runs on the EC2 instance to set up client directories with random files
# Configuration
NUM_CLIENTS=5  # Number of client directories to create
TORRENTS_SOURCE="$HOME/torrents"  # Source directory for torrent files
DOWNLOADS_SOURCE="$HOME/downloads"  # Source directory for download files
MIN_TORRENTS=6  # Minimum number of torrents per client
MAX_TORRENTS=6 # Maximum number of torrents per client
MIN_DOWNLOADS=6  # Minimum number of downloads per client
MAX_DOWNLOADS=6  # Maximum number of downloads per client

# Function to get a random number between min and max (inclusive)
random_number() {
    local min=$1
    local max=$2
    echo $(( $RANDOM % ($max - $min + 1) + $min ))
}

# Function to get random files from a directory
random_files() {
    local source_dir=$1
    local count=$2
    
    # Check if source directory exists and has files
    # if [ ! -d "$source_dir" ] || [ -z "$(ls -A $source_dir)" ]; then
    #     echo "Error: Source directory $source_dir doesn't exist or is empty"
    #     return 1
    # fi
    
    # Get all files in the source directory
    local files=($(ls -A "$source_dir"))
    local total_files=${#files[@]}
    
    # Adjust count if it's more than available files
    if [ $count -gt $total_files ]; then
        count=$total_files
    fi
    
    # Select random files
    local selected=()
    local indices=()
    
    # Create an array of indices
    for ((i=0; i<$total_files; i++)); do
        indices[$i]=$i
    done
    
    # Shuffle the indices
    for ((i=$total_files-1; i>=0; i--)); do
        local j=$(( $RANDOM % ($i + 1) ))
        local temp=${indices[$i]}
        indices[$i]=${indices[$j]}
        indices[$j]=$temp
    done
    
    # Select the first 'count' files
    for ((i=0; i<$count; i++)); do
        selected[$i]=${files[${indices[$i]}]}
    done
    
    echo "${selected[@]}"
}

# Remove existing client directories
echo "Removing existing client directories..."
sudo rm -rf client*_data client*_torrents client*_downloads

# Create new client directories
echo "Creating new client directories..."
for i in $(seq 1 $NUM_CLIENTS); do
    sudo mkdir -p client${i}_data client${i}_torrents client${i}_downloads
    
    # Determine random number of torrents and downloads for this client
    num_torrents=$(random_number $MIN_TORRENTS $MAX_TORRENTS)
    num_downloads=$(random_number $MIN_DOWNLOADS $MAX_DOWNLOADS)
    
    echo "Client $i: Copying $num_torrents torrents and $num_downloads downloads"
    
    # Get random torrent files
    torrent_files=($(random_files "$TORRENTS_SOURCE" $num_torrents))
    
    # Copy torrent files to client directory
    for file in "${torrent_files[@]}"; do
        sudo cp "$TORRENTS_SOURCE/$file" client${i}_torrents/
        echo "  Copied torrent: $file"
    done
    
    # Get random download files
    download_files=($(random_files "$DOWNLOADS_SOURCE" $num_downloads))
    
    # Copy download files to client directory
    for file in "${download_files[@]}"; do
        sudo cp "$DOWNLOADS_SOURCE/$file" client${i}_downloads/
        echo "  Copied download: $file"
    done
done

# Set appropriate permissions
echo "Setting permissions..."
sudo chmod -R 777 client*

echo "Client directory setup complete!"