#pragma once


#include "stdafx.h"

#include "GlobalVariables.h"


using namespace std;


extern FILE *wEventLog;
extern bool hardrockFlip;


inline bool isIn(float a, float b, float c) {
	return (b > a && b < c) || (b < a && b > c);
}

inline float lerp(float a, float b, float t) {
	return a * (1.f - t) + b * t;
}

inline long BinomialCoefficient(const int &n, int k) {
	if (k < 0 || k > n) { return 0; }
	if (k == 0 || k == n) { return 1; }
	k = MIN(k, n - k);
	long c = 1;
	for (int i = 0; i < k; i++) {
		c = c * (n - i) / (i + 1);
	} return c;
}

inline float Bernstein(const int &i, const int &n, const float &t) {
	return BinomialCoefficient(n, i) * powf(t, static_cast<float>(i)) * powf(1.f - t, static_cast<float>(n) - static_cast<float>(i));
}

inline vec2f PolyBezier(const vector<vec2f> &pts, const int &cp, const int &r, const float &t) {
	vec2f c;
	for (int i = 0; i <= cp; i++) {
		float b = Bernstein(i, cp, t);
		c.y += pts.at(i + (r * cp)).y * b;
		c.x += pts.at(i + (r * cp)).x * b;
	} return c;
}

inline vec2f Intersect(vec2f a, vec2f ta, vec2f b, vec2f tb) {
	float des = tb.x * ta.y - tb.y * ta.x;
	if (abs(des) < 0.00001f) { /* EventLog */  fwprintf(wEventLog, L"[ERROR]  Intersect --> Vectors are parallel.\n"); fflush(wEventLog); }
	float u = ((b.y - a.y) * ta.x + (a.x - b.x) * ta.y) / des;
	return b.cpy().add(tb.x * u, tb.y * u);
}


inline vec2f CirclePoint(vec2f center, float radius, float angle) {
	float x = cosf(angle) * radius;
	float y = sinf(angle) * radius;
	return vec2f(x, y) + center;
}


class Segment {
public: vector<vec2f> points;

		~Segment() {}
};

class TimingPoint {
	int TimingTime;
	float BPM;
public:
	explicit TimingPoint(wstring TimingString) {
		vector<wstring> tokens = split_string(TimingString, L",");
		TimingTime = stoi(tokens.at(0));
		try {
			BPM = stof(tokens.at(1));
		}
		catch (...) {
			wstring bpm = tokens.at(1);
			wstring newBpm;
			if (bpm.find('-') != string::npos) {
				newBpm = L"MIN_FLOAT";
				BPM = 0.f;
			}
			else {
				newBpm = L"MAX_FLOAT";
				BPM = FLT_MAX;
			}
			
			/* EventLog */  fwprintf(wEventLog, (L"[ERROR]  BPM out of Range --> " + bpm + L".\n" + L"            Setting BPM to " + newBpm + L" value.\n").c_str());
			fflush(wEventLog);
		}
	}

	float getBPM() const {
		return BPM;
	}
	int getTime() const {
		return TimingTime;
	}

	~TimingPoint() {}
};

class HitObject {
public:
	vector<vec2f> sliderPoints;
	vector<Segment> sliderSegments;
	vec2f startPosition,
		circleCenter,
		centerP;
	int startTime,
		endTime = 0,
		sliderTime,
		hitType,
		stackId = 0;
	float radius,
		radiusP,
		startAng,
		midAng,
		endAng,
		pixelLength,
		beatLength,
		beatLengthBase,
		repeatCount,
		sliderTickCount,
		timingPointMultiplier;
	wchar_t sliderType;
	
	int getHitType() {
		if ((hitType & 2) > 0)
			return HIT_SLIDER;
		else if ((hitType & 8) > 0)
			return HIT_SPINNER;
		else
			return HIT_CIRCLE;
	}

	int getStartTime() {
		return startTime;
	}

	int getEndTime() {
		return endTime;
	}

	int getStack() {
		return stackId;
	}

	float getSliderTickCount() {
		return sliderTickCount;
	}

	float getSliderRepeatCount() {
		return repeatCount;
	}

	float getSliderTime() {
		return static_cast<float>(sliderTime);
	}

	float getBPM() {
		return beatLength;
	}

	void setStack(int stack) {
		stackId = stack;
	}

	vec2f getStartPos() {
		return startPosition;
	}
	vec2f getEndPos() {
		if (getHitType() != HIT_SLIDER)
			return getStartPos();

		static float t = getSliderRepeatCount();
		return getPointByT(t);
	}

	vec2f getPointByT(float &t) {
		float floor = floorf(t);
		t = static_cast<int>(floor) % 2 == 0 ? t - floor : floor + 1.f - t;

		if (sliderType == 'P') {
			float ang = lerp(startAng, endAng, t);
			if (hardrockFlip)
				return { circleCenter.x + radius * cosf(ang), 384.f - (circleCenter.y + radius * sinf(ang)) };
			return { circleCenter.x + radius * cosf(ang), circleCenter.y + radius * sinf(ang) };
		}

		float dist = pixelLength * t;
		float currDist = 0.f;

		vec2f oldPoint;
		try { oldPoint = sliderSegments.at(0).points.at(0); }
		catch (...) {
			/* EventLog */	fwprintf(wEventLog, L"[ERROR]  getPointByT --> oldPoint\n");
			fflush(wEventLog);

			if (hardrockFlip)
				return vec2f(oldPoint.x, 384.f - oldPoint.y);
			return oldPoint;
		}

		for (unsigned int i = 0; i < sliderSegments.size(); i++) {
			auto seg = sliderSegments.at(i);
			if (i == sliderSegments.size() - 1) {
				float ct = 0.f;
				while (currDist < pixelLength) {
					vec2f p = PolyBezier(seg.points, INT(seg.points.size()) - 1, 0, ct);
					currDist += (oldPoint - p).length();
					
					if (currDist > dist) {
						if (hardrockFlip)
							return vec2f(oldPoint.x, 384.f - oldPoint.y);
						return oldPoint;
					}
					
					oldPoint = p;
					ct += 1.f / (seg.points.size() * 50 - 1);
				}
			}

			for (float ct = 0.f; ct < 1.f + (1.f / (seg.points.size() * 50.f - 1.f)); ct += 1.f / (seg.points.size() * 50.f - 1.f)) {
				vec2f p = PolyBezier(seg.points, INT(seg.points.size()) - 1, 0, ct);

				currDist += (oldPoint - p).length();
				if (currDist > dist) {
					if (hardrockFlip)
						return vec2f(oldPoint.x, 384.f - oldPoint.y);
					return oldPoint;
				}
				oldPoint = p;
			}
		}

		if (hardrockFlip)
			return vec2f(oldPoint.x, 384.f - oldPoint.y);
		return oldPoint;
	}

	HitObject(wstring hitstring, vector<TimingPoint> *timingPoints, float mapSliderMultiplier, float mapSliderTickRate) {
		auto tokens = split_string(hitstring, L",");
		startPosition = vec2f(stof(tokens.at(0)), stof(tokens.at(1)));
		startTime = stoi(tokens.at(2));
		hitType = _wtoi(tokens.at(3).c_str());

		if (getHitType() == HIT_SLIDER) {
			beatLengthBase = timingPoints->at(0).getBPM();
			float BPM = beatLengthBase;
			repeatCount = stof(tokens.at(6));
			pixelLength = stof(tokens.at(7));

			for (auto point : *timingPoints) {
				if (point.getTime() <= startTime) {
					if (point.getBPM() >= 0.f) { beatLengthBase = point.getBPM(); }
					BPM = point.getBPM();
				}
			}

			if (BPM < 0.f) { float newMulti = BPM / -100.f; BPM = beatLengthBase * newMulti; }
			beatLength = BPM;
			timingPointMultiplier = beatLength / beatLengthBase;

			sliderTime = static_cast<int>(BPM * (pixelLength / mapSliderMultiplier) / 100.f);
			endTime = static_cast<int>(static_cast<float>(sliderTime) * repeatCount) + startTime;

			sliderTickCount = pixelLength / (100.f * mapSliderMultiplier / mapSliderTickRate / timingPointMultiplier);
			if (sliderTickCount < 1.f) sliderTickCount = 1.f;

			sliderPoints.push_back(startPosition);

			auto sliderTokens = split_string(tokens.at(5), L"|");
			for (int i = 1; i < static_cast<int>(sliderTokens.size()); i++) {
				auto p = split_string(sliderTokens.at(i), L":");
				vec2f point(stof(p.at(0)), stof(p.at(1)));
				sliderPoints.push_back(point);
			}

			if (sliderPoints.at(sliderPoints.size() - 1) == sliderPoints.at(sliderPoints.size() - 2))
				sliderPoints.resize(sliderPoints.size() - 1);

			sliderType = sliderTokens[0].c_str()[0];

			if (sliderType == L'L' || sliderType == L'C') {
				for (int i = 1; i < static_cast<int>(sliderPoints.size()); i++) {
					Segment seg;
					seg.points = { sliderPoints.at(i - 1), sliderPoints.at(i) };

					sliderSegments.push_back(seg);
				}
			}
			else if (sliderType == L'P' && sliderPoints.size() == 3) {
				vec2f start = sliderPoints.at(0);
				vec2f mid = sliderPoints.at(1);
				vec2f end = sliderPoints.at(2);

				vec2f mida = start.midPoint(mid);
				vec2f midb = end.midPoint(mid);
				vec2f nora = mid.cpy().sub(start).nor();
				vec2f norb = mid.cpy().sub(end).nor();

				circleCenter = Intersect(mida, nora, midb, norb);

				vec2f startAngPoint = start.cpy().sub(circleCenter);
				vec2f midAngPoint = mid.sub(circleCenter);
				vec2f endAngPoint = end.cpy().sub(circleCenter);

				startAng = atan2(startAngPoint.y, startAngPoint.x);
				midAng = atan2(midAngPoint.y, midAngPoint.x);
				endAng = atan2(endAngPoint.y, endAngPoint.x);

				radius = startAngPoint.length();

				if (!isIn(startAng, midAng, endAng)) {
					if (abs(startAng + TWO_PI - endAng) < TWO_PI && isIn(startAng + TWO_PI, midAng, endAng))
						startAng += TWO_PI;
					else if (abs(startAng - (endAng + TWO_PI)) < TWO_PI && isIn(startAng, midAng, endAng + TWO_PI))
						endAng += TWO_PI;
					else if (abs(startAng - TWO_PI - endAng) < TWO_PI && isIn(startAng - TWO_PI, midAng, endAng))
						startAng -= TWO_PI;
					else if (abs(startAng - (endAng - TWO_PI)) < TWO_PI && isIn(startAng, midAng, endAng - TWO_PI))
						endAng -= TWO_PI;
					else {
						/* EventLog */	fwprintf(wEventLog, (L"[ERROR]  Angle error: (" + to_wstring(startAng) + L", " + to_wstring(midAng) + L", " + to_wstring(endAng) + L")\n").c_str());
						fflush(wEventLog);
					}
				}
				float arcAng = pixelLength / radius;
				if (endAng > startAng) endAng = startAng + arcAng;
				else endAng = startAng - arcAng;

				centerP = circleCenter;
				radiusP = radius;
			}
			else {
				sliderType = L'B';
				vector<vector<vec2f>> curveList;
				vector<vec2f> curve;

				for (auto point : sliderPoints) {
					if (curve.size() > 1) {
						if (point == curve.at(curve.size() - 1)) {
							curveList.push_back(curve);
							curve.clear();
						}
					} curve.push_back(point);
				}
				curveList.push_back(curve);
				curve.clear();

				for (auto plot : curveList) {
					Segment seg; seg.points = plot;
					sliderSegments.push_back(seg);
				}
			}
		}
		else if (getHitType() == HIT_SPINNER) endTime = _wtoi(tokens.at(5).c_str());
	}

	~HitObject() {}
};
