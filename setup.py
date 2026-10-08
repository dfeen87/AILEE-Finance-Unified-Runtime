#!/usr/bin/env python3
from setuptools import find_packages, setup

setup(
    name="aille",
    version="24.0.0",
    description="AILLE: AI-Load Integrity and Layered Evaluation",
    author="Don Michael Feeney Jr",
    author_email="dfeen87@gmail.com",
    url="https://github.com/dfeen87/AILEE-Finance-Unified-Runtime",
    # The SDK lives at the repository root. Automatic discovery selects src/,
    # which contains development simulations rather than the runtime packages.
    packages=find_packages(include=("ailee_finance", "ailee_finance.*", "core", "core.*")),
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.8",
)
