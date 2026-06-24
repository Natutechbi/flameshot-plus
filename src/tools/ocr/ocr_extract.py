#!/usr/bin/env python3
"""
Flameshot OCR Extractor - PaddleOCR wrapper.
Called by the OCR tool via QProcess.

Usage: ocr_extract.py <image_path> [--lang <lang>]

Outputs JSON to stdout: {"success": true, "text": "...", "confidence": 0.95}
On failure: {"success": false, "error": "...", "text": ""}
"""

import argparse
import json
import os
import sys


def extract_text(image_path: str, lang: str = "en") -> dict:
    """Run PaddleOCR on the given image and return extracted text."""
    try:
        from paddleocr import PaddleOCR
    except ImportError:
        return {"success": False, "text": "", "error": "PaddleOCR not installed"}

    try:
        # Suppress output noise
        ocr = PaddleOCR(use_angle_cls=True, lang=lang, show_log=False)
        result = ocr.ocr(image_path)
    except Exception as e:
        return {"success": False, "text": "", "error": str(e)}

    # Parse result: each page is a list of [bbox, (text, confidence)]
    lines = []
    confidences = []
    if result:
        for page in result:
            if page is None:
                continue
            for line in page:
                bbox, (text, confidence) = line
                lines.append(text.strip())
                confidences.append(confidence)

    full_text = "\n".join(lines)
    avg_confidence = sum(confidences) / len(confidences) if confidences else 0.0

    return {
        "success": True,
        "text": full_text,
        "confidence": round(avg_confidence, 4),
        "lines": len(lines),
    }


def main():
    parser = argparse.ArgumentParser(
        description="Extract text from image using PaddleOCR"
    )
    parser.add_argument("image_path", help="Path to the image file")
    parser.add_argument("--lang", default="en", help="Language code (default: en)")
    args = parser.parse_args()

    if not os.path.isfile(args.image_path):
        result = {
            "success": False,
            "text": "",
            "error": f"File not found: {args.image_path}",
        }
    else:
        result = extract_text(args.image_path, args.lang)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
