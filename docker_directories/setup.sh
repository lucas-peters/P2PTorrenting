#!/bin/bash
# setup_directories.sh - Create directory structure for P2P clients

# Create bootstrap data directory
mkdir -p bootstrap_data

# Create directories for each client
for i in {1..3}; do
  mkdir -p client${i}_torrents
  mkdir -p client${i}_downloads
  mkdir -p client${i}_data
done

echo "Directory structure created successfully!"