#!/usr/bin/env python3

"""Generate test images and videos for VidMod.

This script creates:
1. Three still images with different sizes and color themes.
2. Three videos with time-varying color themes (default: 180 seconds each).

Dependencies:
- numpy
- opencv-python

Example:
    python3 generate_test_media.py

Fast test run:
    python3 generate_test_media.py --duration 12 --fps 24 --video-size 960x540
"""

from __future__ import annotations

import argparse
import math
from pathlib import Path

try:
    import cv2  # type: ignore
    import numpy as np
except ImportError as exc:  # pragma: no cover
    raise SystemExit(
        "Missing dependencies. Install with: pip install numpy opencv-python"
    ) from exc


def parse_size(value: str) -> tuple[int, int]:
    parts = value.lower().split("x")
    if len(parts) != 2:
        raise argparse.ArgumentTypeError("Size must be WIDTHxHEIGHT (e.g. 1280x720)")
    try:
        width = int(parts[0])
        height = int(parts[1])
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Size must contain integer values") from exc
    if width <= 0 or height <= 0:
        raise argparse.ArgumentTypeError("Width and height must be positive")
    return width, height


class ThemeField:
    def __init__(self, width: int, height: int) -> None:
        x = np.linspace(-1.0, 1.0, width, dtype=np.float32)
        y = np.linspace(-1.0, 1.0, height, dtype=np.float32)
        self.xx, self.yy = np.meshgrid(x, y)
        self.radius = np.sqrt(self.xx * self.xx + self.yy * self.yy)
        self.angle = np.arctan2(self.yy, self.xx)
        self.width = width
        self.height = height


def hsv_to_bgr8(h: np.ndarray, s: np.ndarray, v: np.ndarray) -> np.ndarray:
    h_u8 = np.mod(h * 179.0, 179.0).astype(np.uint8)
    s_u8 = np.clip(s * 255.0, 0.0, 255.0).astype(np.uint8)
    v_u8 = np.clip(v * 255.0, 0.0, 255.0).astype(np.uint8)
    hsv = np.dstack((h_u8, s_u8, v_u8))
    return cv2.cvtColor(hsv, cv2.COLOR_HSV2BGR)


def theme_sunset(field: ThemeField, t: float) -> np.ndarray:
    wave = np.sin(3.4 * field.xx + 2.6 * field.yy + t * 2.4)
    band = np.cos(7.0 * field.yy - t * 1.6)
    hue = 0.02 + 0.13 * (0.5 + 0.5 * wave) + 0.03 * (0.5 + 0.5 * band)
    sat = 0.65 + 0.35 * (0.5 + 0.5 * np.sin(field.radius * 8.0 - t * 1.2))
    val = 0.45 + 0.55 * (0.5 + 0.5 * np.cos(field.yy * 4.0 + t * 1.8))
    return hsv_to_bgr8(hue, sat, val)


def theme_ocean(field: ThemeField, t: float) -> np.ndarray:
    swirl = np.sin(5.0 * field.radius - t * 2.1 + 2.2 * field.angle)
    ripple = np.cos(8.5 * field.xx + t * 1.4)
    hue = 0.45 + 0.14 * (0.5 + 0.5 * swirl) + 0.03 * (0.5 + 0.5 * ripple)
    sat = 0.55 + 0.35 * (0.5 + 0.5 * np.sin(6.0 * field.yy - t * 1.7))
    val = 0.38 + 0.62 * (0.5 + 0.5 * np.cos(4.5 * field.radius + t * 2.5))
    return hsv_to_bgr8(hue, sat, val)


def theme_neon(field: ThemeField, t: float) -> np.ndarray:
    stripes = np.sin(16.0 * field.xx + t * 3.0)
    rings = np.cos(20.0 * field.radius - t * 2.0)
    drift = np.sin(9.0 * field.yy + t * 2.6)
    hue = np.mod(0.75 + 0.25 * stripes + 0.12 * rings + 0.08 * drift, 1.0)
    sat = 0.72 + 0.28 * (0.5 + 0.5 * np.cos(10.0 * field.angle + t * 2.1))
    val = 0.28 + 0.72 * (0.5 + 0.5 * rings)
    return hsv_to_bgr8(hue, sat, val)


THEMES = {
    "sunset": theme_sunset,
    "ocean": theme_ocean,
    "neon": theme_neon,
}


def write_image(path: Path, width: int, height: int, theme_name: str) -> None:
    field = ThemeField(width, height)
    image = THEMES[theme_name](field, t=0.0)
    if not cv2.imwrite(str(path), image):
        raise RuntimeError(f"Failed to write image: {path}")


def write_video(
    path: Path,
    width: int,
    height: int,
    fps: int,
    duration_seconds: int,
    theme_name: str,
) -> None:
    frame_total = max(1, fps * duration_seconds)
    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    writer = cv2.VideoWriter(str(path), fourcc, float(fps), (width, height))
    if not writer.isOpened():
        raise RuntimeError(f"Failed to open video writer: {path}")

    field = ThemeField(width, height)

    try:
        print(f"Generating {path.name}: {duration_seconds}s @ {fps} fps ({frame_total} frames)")
        for frame_index in range(frame_total):
            t_norm = frame_index / max(1, frame_total - 1)
            t = t_norm * math.tau * 3.0
            frame = THEMES[theme_name](field, t)
            writer.write(frame)

            if frame_index == 0 or (frame_index + 1) % max(1, fps * 5) == 0:
                pct = int(round((frame_index + 1) * 100.0 / frame_total))
                print(f"  {path.name}: {frame_index + 1}/{frame_total} ({pct}%)")
    finally:
        writer.release()


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate VidMod test images and videos.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("test_media"),
        help="Output directory (default: test_media)",
    )
    parser.add_argument(
        "--duration",
        type=int,
        default=180,
        help="Duration in seconds for each test video (default: 180)",
    )
    parser.add_argument(
        "--fps",
        type=int,
        default=30,
        help="Frames per second for videos (default: 30)",
    )
    parser.add_argument(
        "--video-size",
        type=parse_size,
        default=(1280, 720),
        help="Video size as WIDTHxHEIGHT (default: 1280x720)",
    )
    parser.add_argument(
        "--images-only",
        action="store_true",
        help="Generate only images",
    )
    parser.add_argument(
        "--videos-only",
        action="store_true",
        help="Generate only videos",
    )

    args = parser.parse_args()

    if args.duration <= 0:
        raise SystemExit("--duration must be > 0")
    if args.fps <= 0:
        raise SystemExit("--fps must be > 0")
    if args.images_only and args.videos_only:
        raise SystemExit("Use either --images-only or --videos-only, not both")

    output_dir = args.output_dir.expanduser().resolve()
    images_dir = output_dir / "images"
    videos_dir = output_dir / "videos"
    images_dir.mkdir(parents=True, exist_ok=True)
    videos_dir.mkdir(parents=True, exist_ok=True)

    image_specs = [
        ("vidmod_test_sunset_wide_1920x1080.png", 1920, 1080, "sunset"),
        ("vidmod_test_ocean_tall_1080x1920.png", 1080, 1920, "ocean"),
        ("vidmod_test_neon_ultrawide_2048x768.png", 2048, 768, "neon"),
    ]

    video_width, video_height = args.video_size
    video_specs = [
        ("vidmod_test_video_sunset.mp4", "sunset"),
        ("vidmod_test_video_ocean.mp4", "ocean"),
        ("vidmod_test_video_neon.mp4", "neon"),
    ]

    print(f"Output directory: {output_dir}")

    if not args.videos_only:
        print("Creating test images...")
        for filename, width, height, theme_name in image_specs:
            path = images_dir / filename
            write_image(path, width, height, theme_name)
            print(f"  Wrote {path}")

    if not args.images_only:
        print("Creating test videos...")
        for filename, theme_name in video_specs:
            path = videos_dir / filename
            write_video(path, video_width, video_height, args.fps, args.duration, theme_name)
            print(f"  Wrote {path}")

    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
