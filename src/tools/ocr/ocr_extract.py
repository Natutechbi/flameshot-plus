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


def merge_into_lines(result, y_threshold=0.35):
    """Group word-level detections into lines by vertical proximity.

    Each page result is a list of [bbox, (text, confidence)] entries where
    bbox is [[x1,y1],[x2,y2],[x3,y3],[x4,y4]]. This merges entries whose
    vertical centers are within y_threshold × avg_height of each other.
    """
    if not result:
        return result

    merged = []
    for page in result:
        if page is None:
            merged.append(None)
            continue

        entries = []
        for bbox, (text, conf) in page:
            ys = [pt[1] for pt in bbox]
            y_c = sum(ys) / 4.0
            y_h = max(ys) - min(ys)
            entries.append((y_c, y_h, bbox, text.strip(), conf))

        if not entries:
            merged.append([])
            continue

        entries.sort(key=lambda e: e[0])

        lines = [[entries[0]]]
        for e in entries[1:]:
            prev = lines[-1][-1]
            gap = abs(e[0] - prev[0])
            max_h = max(e[1], prev[1], 1.0)
            if gap < y_threshold * max_h:
                lines[-1].append(e)
            else:
                lines.append([e])

        page_out = []
        for line in lines:
            line.sort(key=lambda e: e[2][0][0])
            combined = " ".join(e[3] for e in line if e[3])
            avg_conf = sum(e[4] for e in line) / len(line)
            page_out.append([line[0][2], (combined, avg_conf)])

        merged.append(page_out)

    return merged


def extract_text(image_path: str, lang: str = "en") -> dict:
    """Run PaddleOCR on the given image and return extracted text."""
    try:
        from paddleocr import PaddleOCR
    except ImportError:
        return {"success": False, "text": "", "error": "PaddleOCR not installed"}

    try:
        # Tune detection params to favour line-level grouping:
        #   - Lower thresh     → more text pixels in the probability map
        #   - Lower box_thresh → retain more candidate boxes
        #   - Higher unclip_ratio → expand boxes for better overlap merging
        ocr = PaddleOCR(
            use_angle_cls=True,
            lang=lang,
            show_log=False,
            det_db_thresh=0.15,
            det_db_box_thresh=0.3,
            det_db_unclip_ratio=3.0,
        )
        result = ocr.ocr(image_path)
        result = merge_into_lines(result)
    except Exception as e:
        return {"success": False, "text": "", "error": str(e)}

    lines = []
    confidences = []
    if result:
        for page in result:
            if page is None:
                continue
            for bbox, (text, confidence) in page:
                lines.append(text)
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
