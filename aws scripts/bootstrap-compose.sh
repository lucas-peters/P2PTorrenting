#!/bin/bash

# Exit on error
set -e

# Script for deploying P2P Torrenting application on Amazon Linux EC2
echo "P2P Torrenting Deployment Script for Amazon Linux EC2"
echo "======================================================"

# Update system packages
echo "Updating system packages..."
sudo yum update -y

# Install Docker if not already installed
if ! command -v docker &> /dev/null; then
    echo "Installing Docker..."
    sudo yum install -y docker
    sudo systemctl start docker
    sudo systemctl enable docker
    sudo usermod -aG docker ec2-user
    echo "Docker installed successfully!"
else
    echo "Docker is already installed."
fi

# Install Docker Compose if not already installed
if ! command -v docker-compose &> /dev/null; then
    echo "Installing Docker Compose..."
    sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" -o /usr/local/bin/docker-compose
    sudo chmod +x /usr/local/bin/docker-compose
    echo "Docker Compose installed successfully!"
else
    echo "Docker Compose is already installed."
fi

# Create necessary directories
echo "Creating data directories..."
mkdir -p bootstrap_data index_data

# Stop and remove all running containers
echo "Stopping and removing all running containers..."
docker stop $(docker ps -a -q) 2>/dev/null || true
sudo docker rm $(docker ps -a -q) 2>/dev/null || true

# Remove unused Docker resources
echo "Cleaning up Docker resources..."
sudo docker system prune -f

# Pull the latest images
echo "Pulling latest images..."
sudo docker pull lucaspeters/bootstrap:latest
sudo docker pull lucaspeters/index:latest

# Get the public IP of the EC2 instance
echo "Detecting public IP address..."
PUBLIC_IP=$(curl -s http://169.254.169.254/latest/meta-data/public-ipv4)
echo "Public IP: $PUBLIC_IP"

# Export the PUBLIC_IP as an environment variable
export PUBLIC_IP

# Run docker-compose with the bootstrap configuration
echo "Starting containers with docker-compose..."
sudo docker-compose -f docker-compose.bootstrap.yml up -d

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