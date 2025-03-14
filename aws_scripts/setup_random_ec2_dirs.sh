#!/bin/bash

# This script copies torrent and download directories to EC2 instances

SSH_KEY="p2pswarm.pem"
EC2_USER="ec2-user"
IPS_FILE="client_ips.txt"
TORNT="torrents"
DWNLD="downloads"

# Main script
echo "Starting P2P Torrenting EC2 setup..."

# Deploy to EC2 instances
echo "Copying to EC2 instances..."

# Read IPs from file
IPS=($(cat "$IPS_FILE"))

if [ ${#IPS[@]} -eq 0 ]; then
    echo "Error: No IPs found in $IPS_FILE"
    exit 1
fi

for ip in "${IPS[@]}"; do
    echo "Deploying to EC2 instance: $ip"
    
    # Create torrents and downloads directories on the EC2 instance
    echo "  Creating source directories on remote instance..."
    ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no "$EC2_USER@$ip" "mkdir -p ~/torrents ~/downloads"
    ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no "$EC2_USER@$ip" "rm -rf ~/torrents/* && rm -rf ~/downloads/*"
    
    # Copy the torrent and download files to the EC2 instance
    echo "  Copying torrent files to remote instance..."
    scp -i "$SSH_KEY" -o StrictHostKeyChecking=no -r "$TORNT/"* "$EC2_USER@$ip:~/torrents/" 2>/dev/null || true
    
    echo "  Copying download files to remote instance..."
    scp -i "$SSH_KEY" -o StrictHostKeyChecking=no -r "$DWNLD/"* "$EC2_USER@$ip:~/downloads/" 2>/dev/null || true
    
    # Copy the randomization script to the EC2 instance
    echo "  Copying randomization script..."
    scp -i "$SSH_KEY" -o StrictHostKeyChecking=no "$TEMP_DIR/randomize_dirs.sh" "$EC2_USER@$ip:~/"
    
    # Run the randomization script on the EC2 instance
    echo "  Running randomization script on remote instance..."
    ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no "$EC2_USER@$ip" "bash ~/randomize_dirs.sh"
    
    echo "  Deployment to $ip complete"
done

echo "All torrents and downloads copied"