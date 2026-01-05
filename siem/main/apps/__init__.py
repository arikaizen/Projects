"""
Apps module for SIEM
This module provides a plugin system for SIEM apps.
"""

from abc import ABC, abstractmethod
from typing import Dict, Any


class BaseApp(ABC):
    """
    Base class for all SIEM apps.

    All apps must inherit from this class and implement the required methods.
    """

    @property
    @abstractmethod
    def name(self) -> str:
        """Return the display name of the app"""
        pass

    @property
    @abstractmethod
    def id(self) -> str:
        """Return the unique identifier for the app"""
        pass

    @property
    def description(self) -> str:
        """Return a description of the app"""
        return ""

    @property
    def icon(self) -> str:
        """Return an icon or emoji for the app"""
        return "📱"

    @abstractmethod
    def render(self) -> Dict[str, Any]:
        """
        Return the app configuration for rendering.

        Returns:
            Dict containing:
                - html: HTML content for the app
                - css: Optional CSS styles
                - js: Optional JavaScript code
        """
        pass
