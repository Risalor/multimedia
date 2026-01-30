import sys
from PIL import Image
import matplotlib.pyplot as plt

def simple_histogram_bmp(image_path):
    """
    Simple version using only PIL for histogram calculation
    """
    try:
        # Open and convert to grayscale
        img = Image.open(image_path).convert('L')
        
        # Get histogram using PIL's built-in function
        pil_histogram = img.histogram()
        
        # Display image
        plt.figure(figsize=(10, 4))
        
        plt.subplot(1, 2, 1)
        plt.imshow(img, cmap='gray')
        plt.title('Grayscale Image')
        plt.axis('off')
        
        # Plot histogram
        plt.subplot(1, 2, 2)
        plt.bar(range(256), pil_histogram, color='blue', alpha=0.7)
        plt.title('Grayscale Histogram')
        plt.xlabel('Pixel Intensity')
        plt.ylabel('Frequency')
        plt.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.show()
        
        # Print basic info
        print(f"Image size: {img.size}")
        print(f"Image mode: {img.mode}")
        print(f"Most frequent intensity: {pil_histogram.index(max(pil_histogram))}")
        
    except Exception as e:
        print(f"Error: {str(e)}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python simple_histogram.py <image_path.bmp>")
        sys.exit(1)
    
    simple_histogram_bmp(sys.argv[1])