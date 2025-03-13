#!/bin/bash

# Exit on error
set -e

# Path to your key file - replace with your actual key path
KEY_FILE="p2pswarm.pem"
SSH_USER="ec2-user"
PROJECT_DIR="/Users/lucaspeters/Documents/GitHub/P2PTorrenting"
REMOTE_DIR="~/"

# Files to copy
FILES_TO_COPY=(
  #
  "docker-compose.client.yml"
  "client-compose.sh"
  #"randomize_dirs.sh"
)

# Check if key file exists
if [ ! -f $(eval echo $KEY_FILE) ]; then
  echo "Error: SSH key file not found at $KEY_FILE"
  echo "Please update the KEY_FILE variable with the correct path to your AWS key"
  exit 1
fi

# Check if client_ips.txt exists
if [ ! -f "client_ips.txt" ]; then
  echo "Error: client_ips.txt not found"
  exit 1
fi

# Read IPs from client_ips.txt
echo "Reading EC2 instance IPs from client_ips.txt..."
IPS=$(cat "client_ips.txt")

# Loop through each IP
for IP in $IPS; do
  echo "===================================================="
  echo "Deploying to EC2 instance: $IP"
  echo "===================================================="
  
  # Copy files to EC2 instance
  echo "Copying files to $IP..."
  for FILE in "${FILES_TO_COPY[@]}"; do
    FILENAME=$(basename "$FILE")
    echo "  Copying $FILENAME..."
    ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$IP" "rm -rf ~/$FILENAME"
    scp -i "$KEY_FILE" -o StrictHostKeyChecking=no "$FILE" "$SSH_USER@$IP:$REMOTE_DIR/"
  done
  
  # Make client-compose.sh executable and run it
  #echo "Making client-compose.sh executable and running it..."
  #ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$IP" "chmod +x ~/randomize_dirs.sh" #"~/randomize_dirs.sh"
  ssh -i "$KEY_FILE" -o StrictHostKeyChecking=no "$SSH_USER@$IP" "chmod +x ~/client-compose.sh && ~/client-compose.sh"
  
  # && ~/client-compose.sh"
  
  echo "Deployment to $IP completed!"
  echo ""
done

echo "All deployments completed successfully!"
