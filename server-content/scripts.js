// Utility function to set text content safely
function setText(selector, text) {
    const element = document.querySelector(selector);
    if (element) {
        element.textContent = text;
    }
}

// Initialize animations on page load
document.addEventListener('DOMContentLoaded', function () {
    console.log('HTTP Server Status Page Loaded');

    // Stagger animations for status boxes
    const statusBoxes = document.querySelectorAll('.status-box');
    statusBoxes.forEach((box, index) => {
        box.style.animationDelay = `${index * 0.1}s`;
    });

    // Add smooth scroll behavior
    document.documentElement.scrollBehavior = 'smooth';

    // Highlight list items on hover with subtle effects
    const listItems = document.querySelectorAll('.info-list li');
    listItems.forEach((item, index) => {
        item.style.animationDelay = `${index * 0.05}s`;
        item.addEventListener('mouseenter', function () {
            this.style.transform = 'translateX(5px)';
            this.style.transition = 'transform 0.2s ease';
        });
        item.addEventListener('mouseleave', function () {
            this.style.transform = 'translateX(0)';
        });
    });

    // Add timestamp to footer
    const footerText = document.querySelector('footer');
    if (footerText) {
        const now = new Date();
        const timeString = now.toLocaleString();
        const serverStartText = document.createElement('p');
        serverStartText.style.marginTop = '15px';
        serverStartText.style.fontSize = '0.85em';
        serverStartText.style.opacity = '0.8';
        serverStartText.textContent = `Page loaded at: ${timeString}`;
        footerText.appendChild(serverStartText);
    }

    // Animate status indicator
    const statusIndicator = document.querySelector('.status-indicator');
    if (statusIndicator) {
        statusIndicator.addEventListener('click', function () {
            this.style.animation = 'none';
            setTimeout(() => {
                this.style.animation = 'pulse 2s infinite';
            }, 10);
        });
    }

    // Track user interactions for analytics (optional)
    trackInteractions();
});

// Function to track basic user interactions
function trackInteractions() {
    const boxes = document.querySelectorAll('.status-box');
    boxes.forEach(box => {
        box.addEventListener('click', function () {
            console.log('User interacted with:', this.querySelector('h2').textContent);
        });
    });
}

// Function to check server status (can be extended)
function checkServerStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            console.log('Server status:', data);
        })
        .catch(error => {
            console.log('Server status check not available:', error);
        });
}

// Expose functions to global scope if needed
window.checkServerStatus = checkServerStatus;
