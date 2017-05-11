/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Ra��l Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _ORB_MATCHER_H
#define _ORB_MATCHER_H

#include <opencv2/highgui/highgui.hpp>
#include <opencv/cv.h>
#include <vector>

#include "../SLAM/Frame.h"

#ifdef _WIN32
#include <stdint.h>
#else
#include <stdint-gcc.h>
#endif

namespace SLAMRecon {
	class Frame;

	class ORBmatcher
	{
		
	public:
		//ORBmatcher();
		ORBmatcher(float nnratio = 0.6, bool checkOri = true);
		~ORBmatcher();
		
		// TrackWithMotionModel�����ڴ�Last Frame�н���track
		// ��LastFrame��MapPoint����ƥ�䵽CurrentFrame�ϵĵ�
		// th��ʾ���Ұ뾶ϵ��
		int SearchByProjection(Frame &CurrentFrame, const Frame &LastFrame, const float th);

		// ��Ҫ����TrackLocalMap�У���vpMapPoints�Ͽ���ƥ�䵽Frame�ϵĵ�
		// th��ʾ���Ұ뾶ϵ��
		int SearchByProjection(Frame &F, const vector<MapPoint*> &vpMapPoints, const float th = 3);

		// KeyFrame�е�MapPoints�ǹ̶��ģ���ô����Ҫ�����Frame���KeyFrame��match�ĵ�ĸ���
		// ����FeatureVector���٣���Frame��KeyFrame����ͬһ��NodeId��KeyPoint���б�����ѯ�Աȣ��ܴ�̶��ϼ��ٲ��ҵĴ���
		// ��Frame����KeyFrame��ƥ���ϵ�MapPoint���洢��vpMapPointMatches��˳���ӦFrame�е�KeyPoint˳�򣩱�����
		// ����ƥ��ĵ�ĸ���
		int SearchByBoW(KeyFrame *pKF, Frame &F, vector<MapPoint*> &vpMapPointMatches);

		// ������KeyFrame֮���ƥ��ĵ㣬����ֵΪvpMatches12ƥ��ֵ�Լ�returnƥ��ĸ���
		// ÿ��index�ϵ�MapPoint��ʾ��KeyFrame2����KeyFrame1�ڸ�index��ƥ���MapPoint����Ҫ����һ������ֵ
		// ��Ҫ����Loop Closing��ComputeSim3����
		// ����FeatureVector���٣�������KeyFrame����ͬһ��NodeId��KeyPoint���б�����ѯ�Աȣ��ܴ�̶��ϼ��ٲ��ҵĴ���
		int SearchByBoW(KeyFrame *pKF1, KeyFrame* pKF2, vector<MapPoint*> &vpMatches12);

		// Ӧ����relocalisation (Tracking)��
		// Ϊ��ѡ��KeyFrame���Ҹ����ƥ���MapPoint
		// �� Relocalization ��ʹ���Ż�����ҵ���inliers��̫�٣��Ǿ�ʹ������ķ�����һ������window size�ҵ������inliers
		// sAlreadyFound pnp��Ը�KeyFrame�Ѿ��ҵ���
		int SearchByProjection(Frame &CurrentFrame, KeyFrame* pKF, const set<MapPoint*> &sAlreadyFound, const float th, const int ORBdist);

		

		

		

		// Project MapPoints seen in KeyFrame into the Frame and search matches.
		
		// Matching to triangulate new MapPoints. Check Epipolar Constraint.
		int SearchForTriangulation(KeyFrame *pKF1, KeyFrame* pKF2, cv::Mat F12, vector<pair<size_t, size_t> > &vMatchedPairs, const bool bOnlyStereo);
		
		// Project MapPoints into KeyFrame and search for duplicated MapPoints.
		int Fuse(KeyFrame* pKF, const vector<MapPoint*> &vpMapPoints, const float th=3.0);


		
		// ��������ORB descriptor��256λ��֮��ĺ������� 
		static int DescriptorDistance(const cv::Mat &a, const cv::Mat &b);

		// ��Ҫ����Loop Closing��ComputeSim3
		// pKF1��pKF2������KeyFrame��vpMatches12�������SearchByBoW�����������KeyFrame֮���MapPoint�Ĺ�ϵ�������Լ����й��˹��ˣ�ֻ���������������ֵ��ȥ����һЩƥ����쳣ֵ
		// R12��KeyFrame1���KeyFrame2����ת����ͬ��t12��ƽ��������th������ֵ�����ڲ��Ұ뾶
		// 
		// �÷�����Ҫ��ͨ����MapPoint����һ��KeyFrame�϶�Ӧ��KeyPoint���������˫�������
		// ��KeyFrame1��ĳ��MapPoint��KeyFrame2�Ͽ����ҵ�һ��ƥ���KeyPoint�����KeyPoint�϶�Ҳ��Ӧһ��MapPoint�����MapPointȥKeyFrame1Ҳ���ҵ�һ��ƥ���KeyPoint
		// ���KeyPoint���ú��ʼKeyFrame1�ϵ��Ǹ�MapPoint��Ӧ������Ϊ��һ��MapPoint��ƥ���ϵ�
		// ��Ȼ������Ҫ�����µ�MapPointƥ�䣬���Դ����ƥ��Բ�����
		// ���ص����¼����ƥ��Ե���Ŀ��ƥ���ϵҲ�洢��vpMatches12��������
		int SearchBySim3(KeyFrame* pKF1, KeyFrame* pKF2, vector<MapPoint *> &vpMatches12, const cv::Mat &R12, const cv::Mat &t12, const float th);


		// ��Ҫ����Loop Closing��ComputeSim3
		// ��vpPoints�������ҵ�KeyFrame��MapPoint�µ�ƥ���MapPoint
		// vpPoints������pKFƥ���ϵ�KeyFrame����Graph�ھ����е�KeyFrames���������е�MapPoint�ļ���
		int SearchByProjection(KeyFrame* pKF, cv::Mat Scw, const vector<MapPoint*> &vpPoints, vector<MapPoint*> &vpMatched, int th);


		// ��Ҫ����Loop Closing��SearchAndFuse��
		// ��vpPoints�������ҵ�KeyFrame��MapPoint�µ�ƥ���MapPoint������Fuse
		// vpPoints������pKFƥ���ϵ�KeyFrame����Graph�ھ����е�KeyFrames���������е�MapPoint�ļ���
		int Fuse(KeyFrame* pKF, cv::Mat Scw, const vector<MapPoint*> &vpPoints, float th, vector<MapPoint *> &vpReplacePoint);
		

		/*
		// Matching for the Map Initialization (only used in the monocular case)
		int SearchForInitialization(Frame &F1, Frame &F2, vector<cv::Point2f> &vbPrevMatched, vector<int> &vnMatches12, int windowSize = 10);
		*/
		

	public:

		static const int TH_LOW;
		static const int TH_HIGH;
		static const int HISTO_LENGTH;

		protected:
			
			// ����ֱ��ͼ������������index
			void ComputeThreeMaxima(vector<int>* histo, const int L, int &ind1, int &ind2, int &ind3);

			// ���ݴ���Ľ�����ֵ�ָ�
			float RadiusByViewingCos(const float &viewCos);

			//
			bool CheckDistEpipolarLine(const cv::KeyPoint &kp1, const cv::KeyPoint &kp2, const cv::Mat &F12, const KeyFrame *pKF);

			// �ҵ���ƥ���Ӧ���Ǿ��������Եĵ㣬��ô�Դ��ڵ�KeyPoint��˵�����µ�Frame�ҵ���Ӧ��KeyPoint
			// һ�����ҵ������KeyPoint��ԭʼ��KeyPoint�ĺ�������ҪС��һ������ֵ��TH_LOW��,�����ҵ�����������С��һ������ƥ��KeyPoint
			// ��һ�����ҵ����������������С��KeyPoint��Ҫ�����ֶȣ�Ҳ���Ǻͺ�������ڶ�С��KeyPoint�ĺ�������Ҫ��һ���Ĳ��
			// �����������Ϊ��˵��������ֶȣ���С�ľ���Ҫ��mfNNratio*�ڶ�С�ľ���С�����KeyPoint����������������
			// ֵԽ��˵�����ֶ�ԽС���������ԽС
			float mfNNratio;

			// ������˵��һ��ͼƬ����õ�KeyPoint����һ��ͼƬ��ƥ���KeyPoint
			// ����һ��ƥ���KeyPoint��ԵĽǶ�Ӧ����һ���ģ������ͼƬ�仯����ԽǶ�
			// �����������Ϊ�˼���ǲ�������ҵ���ƥ��㣬��û�в��������һ���Թ��ɵ�
			// �����Ͼʹ����ս����ȥ��
			bool mbCheckOrientation;

	};
}
#endif