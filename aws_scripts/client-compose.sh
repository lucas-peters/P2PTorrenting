#!/bin/bash

# Exit on error
set -e

# Script for deploying P2P Torrenting application on Amazon Linux EC2
echo "P2P Torrenting Deployment Script for Amazon Linux EC2"
echo "======================================================"

# Stop and remove all running containers
echo "Stopping and removing all running containers..."
sudo docker stop $(sudo docker ps -a -q) 2>/dev/null || true
sudo docker rm $(sudo docker ps -a -q) 2>/dev/null || true

# Remove unused Docker resources
echo "Cleaning up Docker resources..."
sudo docker system prune -f

# Function to check if an image is up to date
check_image_update() {
  local image_name=$1
  echo "Checking if $image_name is up to date..."
  
  # Get the local image ID if it exists
  local_digest=$(sudo docker images --digests --format "{{.Digest}}" $image_name 2>/dev/null || echo "")
  
  if [ -z "$local_digest" ]; then
    echo "Image $image_name not found locally. Pulling..."
    sudo docker pull $image_name
    return
  fi
  
  # Try to use manifest inspect to check for updates
  if sudo docker manifest inspect $image_name >/dev/null 2>&1; then
    # Pull the image manifest without downloading the image
    remote_digest=$(sudo docker manifest inspect $image_name | grep -m1 "digest" | awk -F'"' '{print $4}')
    
    if [ "$local_digest" != "$remote_digest" ]; then
      echo "Image $image_name needs updating. Pulling..."
      sudo docker pull $image_name
    else
      echo "Image $image_name is already up to date. Skipping pull."
    fi
  else
    # Fallback method if manifest inspect is not available
    echo "Docker manifest inspect not available, using fallback method..."
    # Use docker pull with --quiet and check the output
    pull_output=$(sudo docker pull --quiet $image_name 2>&1)
    
    # Check if the output contains "Image is up to date" or similar
    if echo "$pull_output" | grep -q "Image is up to date"; then
      echo "Image $image_name is already up to date. Skipping pull."
    else
      echo "Image $image_name updated."
    fi
  fi
}

# Check and pull the latest images if needed
check_image_update "lucaspeters/client:latest"

# Get the public IP of the EC2 instance
echo "Detecting public IP address..."
PUBLIC_IP=$(curl -s http://checkip.amazonaws.com)
echo "Public IP: $PUBLIC_IP"

# Export the PUBLIC_IP as an environment variable
export PUBLIC_IP

# Run docker-compose with the bootstrap configuration
echo "Starting containers with docker-compose..."
sudo docker-compose -f docker-compose.client.yml up -d

# Check if containers are running
echo "Checking container status..."
sudo docker ps

echo "Deployment completed successfully!"
echo "Bootstrap node is running at $PUBLIC_IP:6881"
echo "Index node is running at $PUBLIC_IP:6885"
echo ""
echo "To view logs:"
echo "  docker logs bootstrap"
echo "  docker logs index"