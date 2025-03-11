#!/bin/bash

# Exit on error
set -e

# Set your image details
IMAGE_TYPES=("bootstrap" "client" "index")
TAG="latest"
REGISTRY="lucaspeters"  # Your Docker registry/username

# Create and use a buildx builder
echo "Creating and using buildx builder..."
docker buildx create --name amd64-builder --use || echo "Builder already exists"
docker buildx inspect --bootstrap

# Build and push each image type
for image_type in "${IMAGE_TYPES[@]}"; do
  echo "Building and pushing ${image_type} image for AMD64..."
  
  # Build and push for AMD64
  docker buildx build -f "Dockerfile.${image_type}" \
    --platform linux/amd64 \
    -t "${REGISTRY}/${image_type}:${TAG}" \
    --push .
    
  echo "Successfully built and pushed ${REGISTRY}/${image_type}:${TAG}"
done

echo "All images built and pushed successfully!"