#!/bin/bash

# Exit on error
set -e

# Path to your key file - replace with your actual key path
KEY_FILE="../env_stuff/p2pswarm.pem"
SSH_USER="ec2-user"
PROJECT_DIR="/Users/lucaspeters/Documents/GitHub/P2PTorrenting"
REMOTE_DIR="~/"

# Files to copy
FILES_TO_COPY=(
  #
  "${PROJECT_DIR}/aws scripts/bootstrap-compose.sh"
)

# Check if key file exists
if [ ! -f $(eval echo $KEY_FILE) ]; then
  echo "Error: SSH key file not found at $KEY_FILE"
  echo "Please update the KEY_FILE variable with the correct path to your AWS key"
  exit 1
fi

# Check if static_ips.txt exists
if [ ! -f "${PROJECT_DIR}/aws scripts/static_ips.txt" ]; then
  echo "Error: static_ips.txt not found"
  exit 1
fi

# Read IPs from static_ips.txt
echo "Reading EC2 instance IPs from static_ips.txt..."
IPS=$(cat "${PROJECT_DIR}/aws scripts/static_ips.txt")

# Loop through each IP
for IP in $IPS; do
  echo "===================================================="
  echo "Deploying to EC2 instance: $IP"
  echo "===================================================="
  
#   # Create remote directory if it doesn't exist
#   echo "Creating remote directory..."
#   ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$IP" "mkdir -p $REMOTE_DIR"
  
  # Copy files to EC2 instance (SCP will overwrite by default)
  echo "Copying deployment files to $IP (will overwrite existing files)..."
  for FILE in "${FILES_TO_COPY[@]}"; do
    echo "  Copying $(basename "$FILE")..."
    # Using -f flag to force overwrite without prompting
    scp -i "$KEY_FILE" -o StrictHostKeyChecking=no "$FILE" "$SSH_USER@$IP:$REMOTE_DIR/"
    done
  
  # Make bootstrap-compose.sh executable
  echo "Making bootstrap-compose.sh executable..."
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$IP" "chmod +x $REMOTE_DIR/bootstrap-compose.sh"

  # Run bootstrap-compose.sh
  echo "Running bootstrap-compose.sh on $IP..."
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$IP" "cd $REMOTE_DIR && ./bootstrap-compose.sh"
  
  echo "Deployment to $IP completed!"
  echo ""
done

echo "All deployments completed successfully!"