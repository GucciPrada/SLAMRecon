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

#include "ORBmatcher.h"
#include "../SLAM/MapPoint.h"
#include "../SLAM/KeyFrame.h"
#include "ORBVocabulary.h"

namespace SLAMRecon {

	
	const int ORBmatcher::TH_HIGH = 100;
	const int ORBmatcher::TH_LOW = 50;
	
	const int ORBmatcher::HISTO_LENGTH = 30;

	ORBmatcher::ORBmatcher(float nnratio, bool checkOri) : mfNNratio(nnratio), mbCheckOrientation(checkOri)
	{
	}

	ORBmatcher::~ORBmatcher() {

	}

	int ORBmatcher::SearchByProjection(Frame &CurrentFrame, const Frame &LastFrame, const float th) {

		int nmatches = 0;

		// Ϊ�˼����תһ����
		vector<int> rotHist[HISTO_LENGTH];
		for (int i = 0; i < HISTO_LENGTH; i++)
			rotHist[i].reserve(500);
		const float factor = 1.0f / HISTO_LENGTH;

		// 
		const cv::Mat Rcw = CurrentFrame.m_Transformation.rowRange(0, 3).colRange(0, 3);
		const cv::Mat tcw = CurrentFrame.m_Transformation.rowRange(0, 3).col(3);
		const cv::Mat twc = -Rcw.t()*tcw;

		const cv::Mat Rlw = LastFrame.m_Transformation.rowRange(0, 3).colRange(0, 3);
		const cv::Mat tlw = LastFrame.m_Transformation.rowRange(0, 3).col(3);
		const cv::Mat tlc = Rlw*twc + tlw; //��ǰ������ĵ���ǰһ���������ϵ�е�λ��

		//const bool bForward = tlc.at<float>(2) > CurrentFrame.mb;
		//const bool bBackward = -tlc.at<float>(2) > CurrentFrame.mb;
		// Current Frame���Last Frame��ǰ�����Ǻ���
		//Hao:(����ʵ�����е����⣬������tlc��ֵһ��Ϊ+-�����(��)��������bForward��bBackwardʼ��Ϊfalse?)
		const bool bForward = tlc.at<float>(2) > 40;
		const bool bBackward = -tlc.at<float>(2) > 40;

		for (int i = 0; i < LastFrame.m_nKeys; i++) {
			MapPoint* pMP = LastFrame.m_vpMapPoints[i];

			if (pMP) {
				if (!LastFrame.m_vbOutlier[i]) {

					// Last Frame�ϵ�MapPointͶӰ��Current Frameƽ����
					cv::Mat x3Dw = pMP->GetWorldPos();
					cv::Mat x3Dc = Rcw*x3Dw + tcw;

					const float xc = x3Dc.at<float>(0);
					const float yc = x3Dc.at<float>(1);
					const float invzc = 1.0 / x3Dc.at<float>(2);

					if (invzc < 0)
						continue;

					//�ҳ���lastframe����Ч��mappoint��currentframe�ϵ�����λ�ã�Ϊu��v
					float u = CurrentFrame.m_pCameraInfo->m_fx*xc*invzc + CurrentFrame.m_pCameraInfo->m_cx;
					float v = CurrentFrame.m_pCameraInfo->m_fy*yc*invzc + CurrentFrame.m_pCameraInfo->m_cy;

					// �Ƿ���Current FrameͼƬ�߽���
					if (u<CurrentFrame.m_pCameraInfo->m_nMinX || u>CurrentFrame.m_pCameraInfo->m_nMaxX)
						continue;
					if (v<CurrentFrame.m_pCameraInfo->m_nMinY || v>CurrentFrame.m_pCameraInfo->m_nMaxY)
						continue;

					// ����Scale Level����Ұ뾶��LevelԽС��������ҵ�ԭͼԽ��
					int nLastOctave = LastFrame.m_vKeys[i].octave;

					float radius = th*CurrentFrame.m_pPLevelInfo->m_vScaleFactors[nLastOctave];

					vector<size_t> vIndices2;

					if (bForward)
						vIndices2 = CurrentFrame.GetFeaturesInArea(u, v, radius, nLastOctave);
					else if (bBackward)
						vIndices2 = CurrentFrame.GetFeaturesInArea(u, v, radius, 0, nLastOctave);
					else
						vIndices2 = CurrentFrame.GetFeaturesInArea(u, v, radius, nLastOctave - 1, nLastOctave + 1);

					if (vIndices2.empty())
						continue;

					const cv::Mat dMP = pMP->GetDescriptor();

					int bestDist = 256;
					int bestIdx2 = -1;

					for (vector<size_t>::const_iterator vit = vIndices2.begin(), vend = vIndices2.end(); vit != vend; vit++) {

						// ����ò��ҵ�KeyPoint�Ѿ���ƥ���MapPoint��ʵ���ϵ�MapPoint��������ʱ��MapPoint���ǾͲ����д���
						// �������ƥ���ϵ���ʱ��MapPoint������ǿ��Ա������ƥ����µ���
						const size_t i2 = *vit;
						if (CurrentFrame.m_vpMapPoints[i2])
							if (CurrentFrame.m_vpMapPoints[i2]->Observations() > 0)
								continue;

						const cv::Mat &d = CurrentFrame.m_Descriptors.row(i2);

						const int dist = DescriptorDistance(dMP, d);

						if (dist < bestDist) {
							bestDist = dist;
							bestIdx2 = i2;
						}
					}

					if (bestDist <= TH_HIGH) {
						CurrentFrame.m_vpMapPoints[bestIdx2] = pMP;
						nmatches++;

						if (mbCheckOrientation) {
							float rot = LastFrame.m_vKeysUn[i].angle - CurrentFrame.m_vKeysUn[bestIdx2].angle;
							if (rot < 0.0)
								rot += 360.0f;
							int bin = round(rot*factor);
							if (bin == HISTO_LENGTH)
								bin = 0;
							assert(bin >= 0 && bin < HISTO_LENGTH);
							rotHist[bin].push_back(bestIdx2);
						}
					}
				}
			}
		}

		if (mbCheckOrientation) {
			int ind1 = -1;
			int ind2 = -1;
			int ind3 = -1;

			ComputeThreeMaxima(rotHist, HISTO_LENGTH, ind1, ind2, ind3);

			for (int i = 0; i < HISTO_LENGTH; i++) {
				if (i != ind1 && i != ind2 && i != ind3) {
					for (size_t j = 0, jend = rotHist[i].size(); j < jend; j++) {
						CurrentFrame.m_vpMapPoints[rotHist[i][j]] = static_cast<MapPoint*>(NULL);
						nmatches--;
					}
				}
			}
		}

		return nmatches;
	}

	int ORBmatcher::SearchByProjection(Frame &F, const vector<MapPoint*> &vpMapPoints, const float th) {

		int nmatches = 0;

		const bool bFactor = th != 1.0;

		// ����LocalMap�е�MapPoint
		for (size_t iMP = 0; iMP < vpMapPoints.size(); iMP++) {

			MapPoint* pMP = vpMapPoints[iMP];

			// �õ㲻������������ֱ�ӷ��أ�isInFrustum������
			if (!pMP->m_bTrackInView)
				continue;

			if (pMP->isBad())
				continue;

			const int &nPredictedLevel = pMP->m_nTrackScaleLevel;

			// �������߼н����жϲ��Ұ뾶
			float r = RadiusByViewingCos(pMP->m_TrackViewCos);

			// �������洫���ϵ��
			if (bFactor)
				r *= th;

			// �ж�֮ǰ�жϵ��ǲ��Ǻ������������������ģ����ܱ�֤��õ�Scale Level�ǺϷ���
			if (nPredictedLevel >= F.m_pPLevelInfo->m_vScaleFactors.size()) 
				continue;

			// �ҵ�Frame�ڸ÷�Χ�ڵ�KeyPoint��index���ϣ�ͬʱ�޶���ȡ��KeyPoint��Level
			const vector<size_t> vIndices =
				F.GetFeaturesInArea(pMP->m_TrackProjX, pMP->m_TrackProjY, r*F.m_pPLevelInfo->m_vScaleFactors[nPredictedLevel], nPredictedLevel - 1, nPredictedLevel);

			if (vIndices.empty())
				continue;

			const cv::Mat MPdescriptor = pMP->GetDescriptor();

			int bestDist = 256;
			int bestLevel = -1;
			int bestDist2 = 256;
			int bestLevel2 = -1;
			int bestIdx = -1;

			// Ϊ��MapPoint������õ�һ��ƥ���KeyPoint
			for (vector<size_t>::const_iterator vit = vIndices.begin(), vend = vIndices.end(); vit != vend; vit++) {
				const size_t idx = *vit;

				// �жϸ�index�ϵ�MapPoint�Ƿ��Ѿ�ƥ����ĳ���Ϸ���MapPoint�ˣ����ƥ���ϾͲ����д������ǲ�������Щ�½�����ʱ��MapPoint
				if (F.m_vpMapPoints[idx])
					if (F.m_vpMapPoints[idx]->Observations() > 0)
						continue;

				const cv::Mat &d = F.m_Descriptors.row(idx);

				const int dist = DescriptorDistance(MPdescriptor, d);

				if (dist < bestDist) {
					bestDist2 = bestDist;
					bestDist = dist;
					bestLevel2 = bestLevel;
					bestLevel = F.m_vKeysUn[idx].octave;
					bestIdx = idx;
				} else if (dist<bestDist2) {
					bestLevel2 = F.m_vKeysUn[idx].octave;
					bestDist2 = dist;
				}
			}

			// ��������ƶ�����һ������������Ϊ����ƥ���ϣ�ͬʱ��Ҫ��֤��һ�͵ڶ����Ƶ�KeyPoint����ͬһ��Level
			if (bestDist <= TH_HIGH) {
				if (bestLevel == bestLevel2 && bestDist > mfNNratio*bestDist2)
					continue;

				F.m_vpMapPoints[bestIdx] = pMP;
				nmatches++;
			}
		}

		return nmatches;
	}

	float ORBmatcher::RadiusByViewingCos(const float &viewCos) {
		if (viewCos > 0.998)
			return 2.5;
		else
			return 4.0;
	}

	int ORBmatcher::SearchByBoW(KeyFrame* pKF, Frame &F, vector<MapPoint*> &vpMapPointMatches) {

		// �õ�KeyFrame�ϵ�MapPoint�б���Ȼ������NULL��MapPoint
		const vector<MapPoint*> vpMapPointsKF = pKF->GetMapPointMatches();

		// �Է��ص�MapPoint�б���г�ʼ����˳���ӦF�е�KeyPoint��˳�򣬳�ʼֵΪNULL
		vpMapPointMatches = vector<MapPoint*>(F.m_nKeys, static_cast<MapPoint*>(NULL));

		// �õ�KeyFrame��FeatureVector�����ٱ�����ֻ�Ա���ͬһ��NodeId�ϵ�KeyPoint�����ٲ�ѯ����ΪFeatureVector��KeyPoint��descriptor��һһ��Ӧ��
		const DBoW2::FeatureVector &vFeatVecKF = pKF->m_FeatVec;

		// ����ͳ��ƥ���KyePoint�����ƫ�Ƶ�angle�Ƕȣ���Ϊ����һ�������˵��ƫ�ƽǶ�Ӧ�÷���һ��Ĺ��ɣ����Ե�Ӧ�ö����
		vector<int> rotHist[HISTO_LENGTH];
		for (int i = 0; i < HISTO_LENGTH; i++)
			rotHist[i].reserve(500);
		const float factor = 1.0f / HISTO_LENGTH;

		// �ֱ�õ�FeatureVector������
		DBoW2::FeatureVector::const_iterator KFit = vFeatVecKF.begin();
		DBoW2::FeatureVector::const_iterator Fit = F.m_FeatVec.begin();
		DBoW2::FeatureVector::const_iterator KFend = vFeatVecKF.end();
		DBoW2::FeatureVector::const_iterator Fend = F.m_FeatVec.end();

		int nmatches = 0;

		// ��FeatureVector�Ľ��б���
		while (KFit != KFend && Fit != Fend) {
		
			// KeyFrame �� Frame ������ĳ��VocTree��NodeId��first����NodeId
			if (KFit->first == Fit->first) {
			
				// ��KeyFrame�ϸ�NodeId��Ӧ��KeyPoint��MapPoint����index ����
				const vector<unsigned int> vIndicesKF = KFit->second; 

				// ��Frame�ϸ�NodeId��Ӧ��KeyPoint��MapPoint����index ����
				const vector<unsigned int> vIndicesF = Fit->second;

				// ������index���б�������ѯ��ĳ����Ч��KeyFrame��MapPoint��˵���Ƿ����һ����֮ƥ���Frame�ϵ�MapPoint��KeyPoint��descriptor��������һ��������
				for (size_t iKF = 0; iKF < vIndicesKF.size(); iKF++) {
				
					// KeyFrame��ĳ��MapPoint��index
					const unsigned int realIdxKF = vIndicesKF[iKF]; 

					// �õ�����Ч��MapPoint����
					MapPoint* pMP = vpMapPointsKF[realIdxKF];

					if (!pMP)
						continue;
					if (pMP->isBad())
						continue;

					// ��MapPoint��Ӧ��KeyPoint��descriptor��256 bit
					const cv::Mat &dKF = pKF->m_Descriptors.row(realIdxKF);

					// ����Frame��NodeId�ϵ����е�KeyPoint����������KeyFrame��descriptor֮�����С�ĺ͵ڶ�С�ĺ�������

					// һ��256λ��descriptor,��һ��keypoint��˵����������������������Ϊ256
					// ����������ʵ������descriptor֮�䲻ͬ��bit�ĸ���
					int bestDist1 = 256; 
					int bestIdxF = -1;
					int bestDist2 = 256;

					for (size_t iF = 0; iF < vIndicesF.size(); iF++) {
						const unsigned int realIdxF = vIndicesF[iF];

						if (vpMapPointMatches[realIdxF]) // ����ýڵ��Ѿ������ݣ�˵���Ѿ����ҵ�������
							continue;

						const cv::Mat &dF = F.m_Descriptors.row(realIdxF);

						const int dist = DescriptorDistance(dKF, dF); // ���㺺������

						if (dist < bestDist1) {
							bestDist2 = bestDist1;
							bestDist1 = dist;
							bestIdxF = realIdxF;
						} else if (dist < bestDist2)
							bestDist2 = dist;
					}

					if (bestDist1 <= TH_LOW) {

						if (static_cast<float>(bestDist1) < mfNNratio*static_cast<float>(bestDist2)) {

							vpMapPointMatches[bestIdxF] = pMP;

							const cv::KeyPoint &kp = pKF->m_vKeysUn[realIdxKF];

							if (mbCheckOrientation) {
								float rot = kp.angle - F.m_vKeys[bestIdxF].angle;
								if (rot < 0.0)
									rot += 360.0f;
								int bin = round(rot*factor);
								if (bin == HISTO_LENGTH)
									bin = 0;
								assert(bin >= 0 && bin < HISTO_LENGTH);
								rotHist[bin].push_back(bestIdxF);
							}
							nmatches++;
						}
					}

				}

				KFit++;
				Fit++;
			} else if (KFit->first < Fit->first) {
				KFit = vFeatVecKF.lower_bound(Fit->first);
			} else {
				Fit = F.m_FeatVec.lower_bound(KFit->first);
			}
		}


		if (mbCheckOrientation) {
			int ind1 = -1;
			int ind2 = -1;
			int ind3 = -1;

			ComputeThreeMaxima(rotHist, HISTO_LENGTH, ind1, ind2, ind3);

			for (int i = 0; i < HISTO_LENGTH; i++) {
				if (i == ind1 || i == ind2 || i == ind3)
					continue;
				for (size_t j = 0, jend = rotHist[i].size(); j < jend; j++) {
					vpMapPointMatches[rotHist[i][j]] = static_cast<MapPoint*>(NULL);
					nmatches--;
				}
			}
		}

		return nmatches;
	}

	int ORBmatcher::SearchByBoW(KeyFrame *pKF1, KeyFrame *pKF2, vector<MapPoint *> &vpMatches12) {

		// ���KeyFrame1�����KeyPoint��FeatureVector��MapPoint�Լ�Descriptors��Ϣ
		const vector<cv::KeyPoint> &vKeysUn1 = pKF1->m_vKeysUn;
		const DBoW2::FeatureVector &vFeatVec1 = pKF1->m_FeatVec;
		const vector<MapPoint*> vpMapPoints1 = pKF1->GetMapPointMatches();
		const cv::Mat &Descriptors1 = pKF1->m_Descriptors;

		// ���KeyFrame2�����KeyPoint��FeatureVector��MapPoint�Լ�Descriptors��Ϣ
		const vector<cv::KeyPoint> &vKeysUn2 = pKF2->m_vKeysUn;
		const DBoW2::FeatureVector &vFeatVec2 = pKF2->m_FeatVec;
		const vector<MapPoint*> vpMapPoints2 = pKF2->GetMapPointMatches();
		const cv::Mat &Descriptors2 = pKF2->m_Descriptors;

		// ���ص�ƥ��KeyFrame1��MapPoint��KeyFrame2��MapPoint��ֵ
		vpMatches12 = vector<MapPoint*>(vpMapPoints1.size(), static_cast<MapPoint*>(NULL));

		// ��־λ��KeyFrame2�ϵ�ĳ��MapPoint�ǲ����Ѿ���ƥ����ˣ�ֻ��ƥ��һ�Σ�һ��һ��ϵ
		vector<bool> vbMatched2(vpMapPoints2.size(), false); 

		// ����ͳ�ƽǶȹ�ϵ
		vector<int> rotHist[HISTO_LENGTH];
		for (int i = 0; i < HISTO_LENGTH; i++)
			rotHist[i].reserve(500);
		const float factor = 1.0f / HISTO_LENGTH;

		int nmatches = 0;

		DBoW2::FeatureVector::const_iterator f1it = vFeatVec1.begin();
		DBoW2::FeatureVector::const_iterator f2it = vFeatVec2.begin();
		DBoW2::FeatureVector::const_iterator f1end = vFeatVec1.end();
		DBoW2::FeatureVector::const_iterator f2end = vFeatVec2.end();

		while (f1it != f1end && f2it != f2end) {

			if (f1it->first == f2it->first) {

				// ��KeyFrame1�ϵ�ĳ��NodeId�µ�MapPoint���б�������Ӧ����KeyFrame2��ͬNodeId�µ�MapPoint
				for (size_t i1 = 0, iend1 = f1it->second.size(); i1 < iend1; i1++) {

					const size_t idx1 = f1it->second[i1];

					MapPoint* pMP1 = vpMapPoints1[idx1];

					if (!pMP1)
						continue;
					if (pMP1->isBad())
						continue;

					const cv::Mat &d1 = Descriptors1.row(idx1);

					int bestDist1 = 256;
					int bestIdx2 = -1;
					int bestDist2 = 256;

					// ��KeyFrame2�ϵ�ĳ��NodeId�µ�MapPoint���б���
					for (size_t i2 = 0, iend2 = f2it->second.size(); i2 < iend2; i2++) {
						const size_t idx2 = f2it->second[i2];

						MapPoint* pMP2 = vpMapPoints2[idx2];

						if (vbMatched2[idx2] || !pMP2)
							continue;

						if (pMP2->isBad())
							continue;

						const cv::Mat &d2 = Descriptors2.row(idx2);

						int dist = DescriptorDistance(d1, d2);

						if (dist < bestDist1) {
							bestDist2 = bestDist1;
							bestDist1 = dist;
							bestIdx2 = idx2;
						} else if (dist < bestDist2) {
							bestDist2 = dist;
						}
					}

					if (bestDist1 < TH_LOW) {

						if (static_cast<float>(bestDist1) < mfNNratio*static_cast<float>(bestDist2)) {

							vpMatches12[idx1] = vpMapPoints2[bestIdx2];
							vbMatched2[bestIdx2] = true;

							if (mbCheckOrientation) {
								float rot = vKeysUn1[idx1].angle - vKeysUn2[bestIdx2].angle;
								if (rot < 0.0)
									rot += 360.0f;
								int bin = round(rot*factor);
								if (bin == HISTO_LENGTH)
									bin = 0;
								assert(bin >= 0 && bin < HISTO_LENGTH);
								rotHist[bin].push_back(idx1);
							}
							nmatches++;
						}
					}
				}

				f1it++;
				f2it++;
			}
			else if (f1it->first < f2it->first) {
				f1it = vFeatVec1.lower_bound(f2it->first);
			}
			else {
				f2it = vFeatVec2.lower_bound(f1it->first);
			}
		}

		if (mbCheckOrientation) {
			int ind1 = -1;
			int ind2 = -1;
			int ind3 = -1;

			ComputeThreeMaxima(rotHist, HISTO_LENGTH, ind1, ind2, ind3);

			for (int i = 0; i < HISTO_LENGTH; i++) {
				if (i == ind1 || i == ind2 || i == ind3)
					continue;
				for (size_t j = 0, jend = rotHist[i].size(); j < jend; j++) {
					vpMatches12[rotHist[i][j]] = static_cast<MapPoint*>(NULL);
					nmatches--;
				}
			}
		}

		return nmatches;
	}

	int ORBmatcher::SearchByProjection(Frame &CurrentFrame, KeyFrame *pKF, const set<MapPoint*> &sAlreadyFound, const float th, const int ORBdist) {

		int nmatches = 0;

		const cv::Mat Rcw = CurrentFrame.m_Transformation.rowRange(0, 3).colRange(0, 3);  // camera pose ���Ż����Ż���� pose
		const cv::Mat tcw = CurrentFrame.m_Transformation.rowRange(0, 3).col(3);
		const cv::Mat Ow = -Rcw.t()*tcw;  // ��ǰ֡�������ϵԭ������������ϵ�е�λ��

		vector<int> rotHist[HISTO_LENGTH];
		for (int i = 0; i < HISTO_LENGTH; i++)
			rotHist[i].reserve(500);
		const float factor = 1.0f / HISTO_LENGTH;

		const vector<MapPoint*> vpMPs = pKF->GetMapPointMatches();

		for (size_t i = 0, iend = vpMPs.size(); i < iend; i++) {

			MapPoint* pMP = vpMPs[i];

			if (pMP) {

				// KeyFrame�Ѿ�ƥ���ϵ�
				if (!pMP->isBad() && !sAlreadyFound.count(pMP)) {  

					// KeyFrame �ϴ��ڵ� MapPoint ����������λ��
					cv::Mat x3Dw = pMP->GetWorldPos();  
					// KeyFrame �ϴ��ڵ� MapPoint ���� current Frame �������λ��
					cv::Mat x3Dc = Rcw*x3Dw + tcw;   

					// KeyFrame �ϴ��ڵ� MapPoint ���� current Frame �������λ�ã� ���� x��y��z
					const float xc = x3Dc.at<float>(0);
					const float yc = x3Dc.at<float>(1);
					const float invzc = 1.0 / x3Dc.at<float>(2);

					// KeyFrame �ϴ��ڵ� MapPoint ���� current Frame ��������λ�ã� ���� x��y��z
					const float u = CurrentFrame.m_pCameraInfo->m_fx*xc*invzc + CurrentFrame.m_pCameraInfo->m_cx;
					const float v = CurrentFrame.m_pCameraInfo->m_fy*yc*invzc + CurrentFrame.m_pCameraInfo->m_cy;

					if (u<CurrentFrame.m_pCameraInfo->m_nMinX || u>CurrentFrame.m_pCameraInfo->m_nMaxX)
						continue;
					if (v<CurrentFrame.m_pCameraInfo->m_nMinY || v>CurrentFrame.m_pCameraInfo->m_nMaxY)
						continue;

					// �жϾ����ǲ�����������
					cv::Mat PO = x3Dw - Ow;
					float dist3D = cv::norm(PO);

					const float maxDistance = pMP->GetMaxDistanceInvariance();
					const float minDistance = pMP->GetMinDistanceInvariance();

					if (dist3D<minDistance || dist3D>maxDistance)
						continue;

					// ��ѯ��ǰ����current frame�е�level
					int nPredictedLevel = pMP->PredictScale(dist3D, CurrentFrame.m_pPLevelInfo->m_fLogScaleFactor);

					// �ж�֮ǰ�жϵ��ǲ��Ǻ������������������ģ����ܱ�֤��õ�Scale Level�ǺϷ���
					if (nPredictedLevel >= CurrentFrame.m_pPLevelInfo->m_vScaleFactors.size())
						continue;

					// Search in a window���ڴ˰뾶��Χ�ڲ��Һ�ѡKeyPoint�㣬descriptors����С��һ����ֵ˵���õ�Ҳ������Match�ϵ�MapPoint��
					const float radius = th*CurrentFrame.m_pPLevelInfo->m_vScaleFactors[nPredictedLevel];

					const vector<size_t> vIndices2 = CurrentFrame.GetFeaturesInArea(u, v, radius, nPredictedLevel - 1, nPredictedLevel + 1);
					if (vIndices2.empty())
						continue;

					const cv::Mat dMP = pMP->GetDescriptor();

					int bestDist = 256;
					int bestIdx2 = -1;

					for (vector<size_t>::const_iterator vit = vIndices2.begin(); vit != vIndices2.end(); vit++) {
						const size_t i2 = *vit;
						if (CurrentFrame.m_vpMapPoints[i2])
							continue;

						const cv::Mat &d = CurrentFrame.m_Descriptors.row(i2);

						const int dist = DescriptorDistance(dMP, d);

						if (dist < bestDist) {
							bestDist = dist;
							bestIdx2 = i2;
						}
					}

					// �Ӻ�ѡ��KeyPoint��ѡ��descript�ĺ���������С�ģ����С�ڸ���ֵ˵����KeyPoint��ӦKeyFrame��MapPoint
					if (bestDist <= ORBdist) {
						CurrentFrame.m_vpMapPoints[bestIdx2] = pMP;
						nmatches++;

						if (mbCheckOrientation) {
							float rot = pKF->m_vKeysUn[i].angle - CurrentFrame.m_vKeysUn[bestIdx2].angle;
							if (rot < 0.0)
								rot += 360.0f;
							int bin = round(rot*factor);
							if (bin == HISTO_LENGTH)
								bin = 0;
							assert(bin >= 0 && bin < HISTO_LENGTH);
							rotHist[bin].push_back(bestIdx2);
						}
					}

				}
			}
		}

		if (mbCheckOrientation) {
			int ind1 = -1;
			int ind2 = -1;
			int ind3 = -1;

			ComputeThreeMaxima(rotHist, HISTO_LENGTH, ind1, ind2, ind3);

			for (int i = 0; i < HISTO_LENGTH; i++) {
				if (i != ind1 && i != ind2 && i != ind3) {
					for (size_t j = 0, jend = rotHist[i].size(); j < jend; j++) {
						CurrentFrame.m_vpMapPoints[rotHist[i][j]] = NULL;
						nmatches--;
					}
				}
			}
		}

		return nmatches;
	}




	int ORBmatcher::SearchForTriangulation(KeyFrame *pKF1, KeyFrame *pKF2, cv::Mat F12,
		vector<pair<size_t, size_t> > &vMatchedPairs, const bool bOnlyStereo)
	{
		const DBoW2::FeatureVector &vFeatVec1 = pKF1->m_FeatVec;
		const DBoW2::FeatureVector &vFeatVec2 = pKF2->m_FeatVec;

		//Compute epipole in second image
		cv::Mat Cw = pKF1->GetCameraCenter(); // ��һ��KeyFrame������ĵ�����������ϵ�е�λ��
		cv::Mat R2w = pKF2->GetRotation();
		cv::Mat t2w = pKF2->GetTranslation();
		cv::Mat C2 = R2w*Cw + t2w; // ��һ��KeyFrame������ĵ��ڵڶ���KeyFrame�������ϵ�е�λ��

		// ���һ��KeyFrame������ĵ��ڵڶ���KeyFrameͼ������ϵ�е�λ��
		const float invz = 1.0f / C2.at<float>(2);
		const float ex = pKF2->m_pCameraInfo->m_fx*C2.at<float>(0)*invz + pKF2->m_pCameraInfo->m_cx;
		const float ey = pKF2->m_pCameraInfo->m_fy*C2.at<float>(1)*invz + pKF2->m_pCameraInfo->m_cy;

		// Find matches between not tracked keypoints
		// Matching speed-up by ORB Vocabulary
		// Compare only ORB that share the same node

		int nmatches = 0;
		vector<bool> vbMatched2(pKF2->m_nKeys, false);
		vector<int> vMatches12(pKF1->m_nKeys, -1);

		vector<int> rotHist[HISTO_LENGTH];
		for (int i = 0; i < HISTO_LENGTH; i++)
			rotHist[i].reserve(500);

		const float factor = 1.0f / HISTO_LENGTH;

		DBoW2::FeatureVector::const_iterator f1it = vFeatVec1.begin();
		DBoW2::FeatureVector::const_iterator f2it = vFeatVec2.begin();
		DBoW2::FeatureVector::const_iterator f1end = vFeatVec1.end();
		DBoW2::FeatureVector::const_iterator f2end = vFeatVec2.end();

		// ������Ժ�����searchbyBow������
		while (f1it != f1end && f2it != f2end) {

			// ��ͬһ��node�²Ž��м��㣬���ټ��㸴�Ӷ�
			if (f1it->first == f2it->first) {

				for (size_t i1 = 0, iend1 = f1it->second.size(); i1 < iend1; i1++) {

					const size_t idx1 = f1it->second[i1];
					MapPoint* pMP1 = pKF1->GetMapPoint(idx1);

					// If there is already a MapPoint skip
					if (pMP1)
						continue;

					const cv::KeyPoint &kp1 = pKF1->m_vKeysUn[idx1];
					const cv::Mat &d1 = pKF1->m_Descriptors.row(idx1);

					int bestDist = TH_LOW;
					int bestIdx2 = -1;

					for (size_t i2 = 0, iend2 = f2it->second.size(); i2 < iend2; i2++) {
						size_t idx2 = f2it->second[i2];

						MapPoint* pMP2 = pKF2->GetMapPoint(idx2);

						// If we have already matched or there is a MapPoint skip
						if (vbMatched2[idx2] || pMP2)
							continue;

				
						const cv::Mat &d2 = pKF2->m_Descriptors.row(idx2);

						const int dist = DescriptorDistance(d1, d2);

						if (dist > TH_LOW || dist > bestDist)
							continue;

						const cv::KeyPoint &kp2 = pKF2->m_vKeysUn[idx2];

						
						const float distex = ex - kp2.pt.x; 
						const float distey = ey - kp2.pt.y;

						// ���㣿��������������������
						if (distex*distex + distey*distey < 100 * pKF2->m_pPLevelInfo->m_vScaleFactors[kp2.octave])
							continue;
						
						// ����㵽��һ�����ߵľ���
						if (CheckDistEpipolarLine(kp1, kp2, F12, pKF2)) {
							bestIdx2 = idx2;
							bestDist = dist;
						}
					}

					if (bestIdx2 >= 0) { 
						const cv::KeyPoint &kp2 = pKF2->m_vKeysUn[bestIdx2];
						vMatches12[idx1] = bestIdx2;
						nmatches++;

						if (mbCheckOrientation) {
							float rot = kp1.angle - kp2.angle;
							if (rot < 0.0)
								rot += 360.0f;
							int bin = round(rot*factor);
							if (bin == HISTO_LENGTH)
								bin = 0;
							assert(bin >= 0 && bin < HISTO_LENGTH);
							rotHist[bin].push_back(idx1);
						}
					}
				}

				f1it++;
				f2it++;
			} else if (f1it->first < f2it->first) {
				f1it = vFeatVec1.lower_bound(f2it->first);
			} else {
				f2it = vFeatVec2.lower_bound(f1it->first);
			}
		}

		if (mbCheckOrientation) {
			int ind1 = -1;
			int ind2 = -1;
			int ind3 = -1;

			ComputeThreeMaxima(rotHist, HISTO_LENGTH, ind1, ind2, ind3);

			for (int i = 0; i < HISTO_LENGTH; i++) {
				if (i == ind1 || i == ind2 || i == ind3)
					continue;
				for (size_t j = 0, jend = rotHist[i].size(); j < jend; j++) {
					vMatches12[rotHist[i][j]] = -1;
					nmatches--;
				}
			}

		}

		vMatchedPairs.clear();
		vMatchedPairs.reserve(nmatches);

		for (size_t i = 0, iend = vMatches12.size(); i < iend; i++)
		{
			if (vMatches12[i] < 0)
				continue;
			vMatchedPairs.push_back(make_pair(i, vMatches12[i]));
		}

		return nmatches;
	}

	int ORBmatcher::Fuse(KeyFrame *pKF, const vector<MapPoint *> &vpMapPoints, const float th) {

		cv::Mat Rcw = pKF->GetRotation();
		cv::Mat tcw = pKF->GetTranslation();

		const float &fx = pKF->m_pCameraInfo->m_fx;
		const float &fy = pKF->m_pCameraInfo->m_fy;
		const float &cx = pKF->m_pCameraInfo->m_cx;
		const float &cy = pKF->m_pCameraInfo->m_cy;
		//const float &bf = pKF->mbf;

		cv::Mat Ow = pKF->GetCameraCenter();

		int nFused = 0;

		const int nMPs = vpMapPoints.size();

		for (int i = 0; i < nMPs; i++) {

			MapPoint* pMP = vpMapPoints[i];

			if (!pMP)
				continue;

			if (pMP->isBad() || pMP->IsInKeyFrame(pKF))
				continue;

			cv::Mat p3Dw = pMP->GetWorldPos();
			cv::Mat p3Dc = Rcw*p3Dw + tcw;

			// Depth must be positive
			if (p3Dc.at<float>(2) < 0.0f)
				continue;

			const float invz = 1 / p3Dc.at<float>(2);
			const float x = p3Dc.at<float>(0)*invz;
			const float y = p3Dc.at<float>(1)*invz;

			const float u = fx*x + cx;
			const float v = fy*y + cy;

			// Point must be inside the image
			if (!pKF->IsInImage(u, v))
				continue;

			//const float ur = u - bf*invz;

			const float maxDistance = pMP->GetMaxDistanceInvariance();
			const float minDistance = pMP->GetMinDistanceInvariance();
			cv::Mat PO = p3Dw - Ow;
			const float dist3D = cv::norm(PO);

			// Depth must be inside the scale pyramid of the image
			if (dist3D<minDistance || dist3D>maxDistance)
				continue;

			// Viewing angle must be less than 60 deg
			cv::Mat Pn = pMP->GetNormal();

			if (PO.dot(Pn) < 0.5*dist3D)
				continue;

			int nPredictedLevel = pMP->PredictScale(dist3D, pKF->m_pPLevelInfo->m_fLogScaleFactor);

			// �ж�֮ǰ�жϵ��ǲ��Ǻ������������������ģ����ܱ�֤��õ�Scale Level�ǺϷ���
			if (nPredictedLevel >= pKF->m_pPLevelInfo->m_vScaleFactors.size())
				continue;

			// Search in a radius
			const float radius = th*pKF->m_pPLevelInfo->m_vScaleFactors[nPredictedLevel];


			const vector<size_t> vIndices = pKF->GetFeaturesInArea(u, v, radius);

			if (vIndices.empty())
				continue;

			// Match to the most similar keypoint in the radius

			const cv::Mat dMP = pMP->GetDescriptor();

			int bestDist = 256;
			int bestIdx = -1;
			for (vector<size_t>::const_iterator vit = vIndices.begin(), vend = vIndices.end(); vit != vend; vit++) {
				const size_t idx = *vit;

				const cv::KeyPoint &kp = pKF->m_vKeysUn[idx];

				const int &kpLevel = kp.octave;

				if (kpLevel<nPredictedLevel - 1 || kpLevel>nPredictedLevel)
					continue;

				
				const float &kpx = kp.pt.x;
				const float &kpy = kp.pt.y;
				const float ex = u - kpx;
				const float ey = v - kpy;
				const float e2 = ex*ex + ey*ey;

				if (e2*pKF->m_pPLevelInfo->m_vInvLevelSigma2[kpLevel] > 5.99)
					continue;
				
				const cv::Mat &dKF = pKF->m_Descriptors.row(idx);

				const int dist = DescriptorDistance(dMP, dKF);

				if (dist<bestDist) {
					bestDist = dist;
					bestIdx = idx;
				}
			}

			// If there is already a MapPoint replace otherwise add new measurement
			if (bestDist <= TH_LOW) {
				MapPoint* pMPinKF = pKF->GetMapPoint(bestIdx);
				if (pMPinKF) {
					if (!pMPinKF->isBad()) {
						if (pMPinKF->Observations()>pMP->Observations())
							pMP->Replace(pMPinKF);
						else
							pMPinKF->Replace(pMP);
					}
				} else {
					pMP->AddObservation(pKF, bestIdx);
					pKF->AddMapPoint(pMP, bestIdx);
				}
				nFused++;
			}
		}

		return nFused;
	}






	

	int ORBmatcher::SearchBySim3(KeyFrame *pKF1, KeyFrame *pKF2, vector<MapPoint*> &vpMatches12, const cv::Mat &R12, const cv::Mat &t12, const float th) {
		
		const float &fx = pKF1->m_pCameraInfo->m_fx;
		const float &fy = pKF1->m_pCameraInfo->m_fy;
		const float &cx = pKF1->m_pCameraInfo->m_cx;
		const float &cy = pKF1->m_pCameraInfo->m_cy;

		// KeyFrame1�����������ϵ����ת�����ƽ������
		cv::Mat R1w = pKF1->GetRotation();
		cv::Mat t1w = pKF1->GetTranslation();

		// KeyFrame2�����������ϵ����ת�����ƽ������
		cv::Mat R2w = pKF2->GetRotation();
		cv::Mat t2w = pKF2->GetTranslation();

		// ��������KeyFrame2�����KeyFrame1����ת�����ƽ������
		cv::Mat R21 = R12.t();
		cv::Mat t21 = -R21*t12;

		// �ֱ�������KeyFrame��MapPoint�Լ���Ӧ����Ŀ
		const vector<MapPoint*> vpMapPoints1 = pKF1->GetMapPointMatches();
		const int N1 = vpMapPoints1.size();

		const vector<MapPoint*> vpMapPoints2 = pKF2->GetMapPointMatches();
		const int N2 = vpMapPoints2.size();

		// ���ݴ����ƥ���vpMatches12��������������������ture��ʾ�ǿ�������ƥ���ϵ�ֵ
		vector<bool> vbAlreadyMatched1(N1, false);
		vector<bool> vbAlreadyMatched2(N2, false);

		for (int i = 0; i < N1; i++) {
			MapPoint* pMP = vpMatches12[i]; // KeyFrame2�ϵ�MapPoint����Ӧ����KeyFrame1��iλ����MapPoint
			if (pMP) {
				vbAlreadyMatched1[i] = true;
				int idx2 = pMP->GetIndexInKeyFrame(pKF2);
				if (idx2 >= 0 && idx2 < N2)
					vbAlreadyMatched2[idx2] = true;
			}
		}

		// KeyFrame1��������MapPoint��ͬʱ��������������ƥ�����ЩMapPoint��ƥ���ϵ�KeyFrame2�е�KeyPoint��index
		vector<int> vnMatch1(N1, -1);
		// KeyFrame2��������MapPoint��ͬʱ��������������ƥ�����ЩMapPoint��ƥ���ϵ�KeyFrame1�е�KeyPoint��index
		vector<int> vnMatch2(N2, -1);

		// ����KeyFrame1��������MapPoint��ͬʱ��������������ƥ�����ЩMapPoint
		// �ҵ���KeyFrame2�п���ƥ���ϸ�MapPoint��KeyPoint
		for (int i1 = 0; i1 < N1; i1++) {

			MapPoint* pMP = vpMapPoints1[i1];

			if (!pMP || vbAlreadyMatched1[i1])
				continue;

			if (pMP->isBad())
				continue;

			cv::Mat p3Dw = pMP->GetWorldPos(); // ��������ϵ��λ��
			cv::Mat p3Dc1 = R1w*p3Dw + t1w; // KeyFrame1�������ϵ��λ��
			cv::Mat p3Dc2 = R21*p3Dc1 + t21; // KeyFrame2�������ϵ��λ��

			// ��KeyFrame2�������ϵ��depth��������ֵ����Ȼ�õ�ʱ�۲첻����
			if (p3Dc2.at<float>(2) < 0.0)
				continue;

			// ����KeyFrame1�ϵĸ�MapPointͶӰ��KeyFrame2��ƽ���ϵĶ�ά���λ��
			const float invz = 1.0 / p3Dc2.at<float>(2);
			const float x = p3Dc2.at<float>(0)*invz;
			const float y = p3Dc2.at<float>(1)*invz;

			const float u = fx*x + cx;
			const float v = fy*y + cy;

			// ͶӰ��Ķ�ά�������KeyFrame��ͼƬ�߽���
			if (!pKF2->IsInImage(u, v))
				continue;

			// �鿴��MapPoint����KeyFrame2�������ϵ�ǲ��������MapPoint�ľ�������
			const float maxDistance = pMP->GetMaxDistanceInvariance();
			const float minDistance = pMP->GetMinDistanceInvariance();
			const float dist3D = cv::norm(p3Dc2);

			if (dist3D<minDistance || dist3D>maxDistance)
				continue;

			// ���Ƹõ�������ڸ�KeyFrame2Ӧ�ô��ڽ�����ģ�͵�Level
			const int nPredictedLevel = pMP->PredictScale(dist3D, pKF2->m_pPLevelInfo->m_fLogScaleFactor);

			// �ж�֮ǰ�жϵ��ǲ��Ǻ������������������ģ����ܱ�֤��õ�Scale Level�ǺϷ���
			if (nPredictedLevel >= pKF2->m_pPLevelInfo->m_vScaleFactors.size())
				continue;

			// ��ǰ����ö�άͶӰ��ĳ���뾶��Χ������ص�KeyPoint
			// LevelԽ����ζ��ͼƬԽС�������ԭͼ�Ͻ��в��ҵĻ��뾶����Ҫ������Ӧ�ķŴ�
			const float radius = th*pKF2->m_pPLevelInfo->m_vScaleFactors[nPredictedLevel];

			// �õ��÷�Χ�ڵ�KeyPoint��index�ļ���
			const vector<size_t> vIndices = pKF2->GetFeaturesInArea(u, v, radius);

			if (vIndices.empty())
				continue;

			// ��MapPointҪ��÷�Χ���������KeyPoint����ƥ�䣬��descriptor�ĺ���������С
			const cv::Mat dMP = pMP->GetDescriptor();

			// �Ը÷�Χ�ڵ�KeyPoint���б�������Ѱ�����MapPoint��ƥ���KeyPoint
			int bestDist = INT_MAX;
			int bestIdx = -1;
			for (vector<size_t>::const_iterator vit = vIndices.begin(), vend = vIndices.end(); vit != vend; vit++) {
				const size_t idx = *vit;

				const cv::KeyPoint &kp = pKF2->m_vKeysUn[idx];

				// ��KeyPoint�Ľ�������Level��������Ƶ�Level�������̫�����С1��Level��
				if (kp.octave<nPredictedLevel - 1 || kp.octave>nPredictedLevel)
					continue;

				const cv::Mat &dKF = pKF2->m_Descriptors.row(idx);

				// ����MapPoint��descriptor��KeyPoint��descriptor�ĺ�������
				const int dist = DescriptorDistance(dMP, dKF);

				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = idx;
				}
			}

			// �������С�ڴ����ֵ����Ϊƥ�����ˣ�����������ı����У������ֵ�ȽϿ���
			if (bestDist <= TH_HIGH) {
				vnMatch1[i1] = bestIdx;
			}
		}

		// ͬ�����KeyFrame2��������MapPoint��ͬʱ��������������ƥ�����ЩMapPoint
		// �ҵ���KeyFrame1�п���ƥ���ϸ�MapPoint��KeyPoint
		// ���̺�������ȫһ��
		for (int i2 = 0; i2 < N2; i2++) {
			MapPoint* pMP = vpMapPoints2[i2];

			if (!pMP || vbAlreadyMatched2[i2])
				continue;

			if (pMP->isBad())
				continue;

			cv::Mat p3Dw = pMP->GetWorldPos();
			cv::Mat p3Dc2 = R2w*p3Dw + t2w;
			cv::Mat p3Dc1 = R12*p3Dc2 + t12;

			if (p3Dc1.at<float>(2) < 0.0)
				continue;

			const float invz = 1.0 / p3Dc1.at<float>(2);
			const float x = p3Dc1.at<float>(0)*invz;
			const float y = p3Dc1.at<float>(1)*invz;

			const float u = fx*x + cx;
			const float v = fy*y + cy;

			// Point must be inside the image
			if (!pKF1->IsInImage(u, v))
				continue;

			const float maxDistance = pMP->GetMaxDistanceInvariance();
			const float minDistance = pMP->GetMinDistanceInvariance();
			const float dist3D = cv::norm(p3Dc1);

			if (dist3D<minDistance || dist3D>maxDistance)
				continue;

			const int nPredictedLevel = pMP->PredictScale(dist3D, pKF1->m_pPLevelInfo->m_fLogScaleFactor);

			// �ж�֮ǰ�жϵ��ǲ��Ǻ������������������ģ����ܱ�֤��õ�Scale Level�ǺϷ���
			if (nPredictedLevel >= pKF1->m_pPLevelInfo->m_vScaleFactors.size())
				continue;

			const float radius = th*pKF1->m_pPLevelInfo->m_vScaleFactors[nPredictedLevel];

			const vector<size_t> vIndices = pKF1->GetFeaturesInArea(u, v, radius);

			if (vIndices.empty())
				continue;

			const cv::Mat dMP = pMP->GetDescriptor();

			int bestDist = INT_MAX;
			int bestIdx = -1;
			for (vector<size_t>::const_iterator vit = vIndices.begin(), vend = vIndices.end(); vit != vend; vit++) {

				const size_t idx = *vit;

				const cv::KeyPoint &kp = pKF1->m_vKeysUn[idx];

				if (kp.octave<nPredictedLevel - 1 || kp.octave>nPredictedLevel)
					continue;

				const cv::Mat &dKF = pKF1->m_Descriptors.row(idx);

				const int dist = DescriptorDistance(dMP, dKF);

				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = idx;
				}
			}

			if (bestDist <= TH_HIGH) {
				vnMatch2[i2] = bestIdx;
			}
		}

		// ���KeyFrame1��MapPoint��Ӧ��KeyFrame2��KeyPoint
		// ͬʱ���ø�KeyPoint��Ӧ��MapPointҲ���ö�Ӧ��KeyFrame1��MapPoint��ӦKeyPoint
		// ��Ϊ������MapPointƥ����
		// nFoundΪ��ƥ���MapPoint�Ķ���
		int nFound = 0;

		for (int i1 = 0; i1 < N1; i1++) {
			int idx2 = vnMatch1[i1];

			if (idx2 >= 0) {
				int idx1 = vnMatch2[idx2];
				if (idx1 == i1) {
					vpMatches12[i1] = vpMapPoints2[idx2];
					nFound++;
				}
			}
		}

		return nFound;
	}

	int ORBmatcher::SearchByProjection(KeyFrame* pKF, cv::Mat Scw, const vector<MapPoint*> &vpPoints, vector<MapPoint*> &vpMatched, int th) {

		// ����ڲ�
		const float &fx = pKF->m_pCameraInfo->m_fx;
		const float &fy = pKF->m_pCameraInfo->m_fy;
		const float &cx = pKF->m_pCameraInfo->m_cx;
		const float &cy = pKF->m_pCameraInfo->m_cy;

		// ת����R��t
		cv::Mat sRcw = Scw.rowRange(0, 3).colRange(0, 3);
		const float scw = sqrt(sRcw.row(0).dot(sRcw.row(0)));
		cv::Mat Rcw = sRcw / scw;
		cv::Mat tcw = Scw.rowRange(0, 3).col(3) / scw;
		cv::Mat Ow = -Rcw.t()*tcw;

		// �洢�Ѿ�ƥ���ϵ�MapPoint
		set<MapPoint*> spAlreadyFound(vpMatched.begin(), vpMatched.end());
		spAlreadyFound.erase(static_cast<MapPoint*>(NULL));

		int nmatches = 0;

		// ������ѡ��MapPoint��������Ѱ���µ�����������ƥ��
		for (int iMP = 0, iendMP = vpPoints.size(); iMP < iendMP; iMP++) {

			MapPoint* pMP = vpPoints[iMP];

			// �����Ѿ��ҵ���
			if (pMP->isBad() || spAlreadyFound.count(pMP))
				continue;

			// �õ���ά�����꣬׼��ͶӰ��KeyFrame�ռ�
			cv::Mat p3Dw = pMP->GetWorldPos();

			// ת�����������ϵ
			cv::Mat p3Dc = Rcw*p3Dw + tcw;

			// depth��������������Ȼ��MapPoint����������ǿ�������
			if (p3Dc.at<float>(2) < 0.0)
				continue;

			// ͶӰ��ͼƬ�ϣ�����ͼƬ������
			const float invz = 1 / p3Dc.at<float>(2);
			const float x = p3Dc.at<float>(0)*invz;
			const float y = p3Dc.at<float>(1)*invz;

			const float u = fx*x + cx;
			const float v = fy*y + cy;

			// ͶӰ��ͼƬ�ϵ����������ͼƬ�߽緶Χ��
			if (!pKF->IsInImage(u, v))
				continue;

			// ��������ĵľ��뻹Ҫ�����MapPoint��Χ���ƣ���Ȼ�ڸ�KeyFrameҲ���Ҳ�����MapPoint��
			const float maxDistance = pMP->GetMaxDistanceInvariance();
			const float minDistance = pMP->GetMinDistanceInvariance();
			cv::Mat PO = p3Dw - Ow;
			const float dist = cv::norm(PO);

			if (dist<minDistance || dist>maxDistance)
				continue;

			// ���㵽������ĵķ���͸�MapPointƽ������н�ҪС��60�ȣ�Ҳ�����ӽ�ҪС��60��
			cv::Mat Pn = pMP->GetNormal();

			if (PO.dot(Pn) < 0.5*dist)
				continue;

			
			// ���Ƹõ�������ڸ�KeyFrame2Ӧ�ô��ڽ�����ģ�͵�Level
			int nPredictedLevel = pMP->PredictScale(dist, pKF->m_pPLevelInfo->m_fLogScaleFactor);

			// �ж�֮ǰ�жϵ��ǲ��Ǻ������������������ģ����ܱ�֤��õ�Scale Level�ǺϷ���
			if (nPredictedLevel >= pKF->m_pPLevelInfo->m_vScaleFactors.size())
				continue;

			// ��ǰ����ö�άͶӰ��KeyFrameĳ���뾶��Χ������ص�KeyPoint
			// LevelԽ����ζ��ͼƬԽС�������ԭͼ�Ͻ��в��ҵĻ��뾶����Ҫ������Ӧ�ķŴ�
			const float radius = th*pKF->m_pPLevelInfo->m_vScaleFactors[nPredictedLevel];

			// // �õ��÷�Χ�ڵ�KeyPoint��index�ļ���
			const vector<size_t> vIndices = pKF->GetFeaturesInArea(u, v, radius);

			if (vIndices.empty())
				continue;

			// ��ǰ��ĺܶ෽��һ���������MapPoint�������������KeyPoint,ͬʱ������һ������ֵ
			// ����Ϊ��MapPoint��KeyPoint��Ӧ��MapPointƥ�����ˣ���ȻҪ�����Ѿ�ƥ���ϵ�MapPoint
			const cv::Mat dMP = pMP->GetDescriptor();

			int bestDist = 256;
			int bestIdx = -1;
			for (vector<size_t>::const_iterator vit = vIndices.begin(), vend = vIndices.end(); vit != vend; vit++) {
				const size_t idx = *vit;
				if (vpMatched[idx])
					continue;

				const int &kpLevel = pKF->m_vKeysUn[idx].octave;

				if (kpLevel<nPredictedLevel - 1 || kpLevel>nPredictedLevel)
					continue;

				const cv::Mat &dKF = pKF->m_Descriptors.row(idx);

				const int dist = DescriptorDistance(dMP, dKF);

				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = idx;
				}
			}

			if (bestDist <= TH_LOW) {
				vpMatched[bestIdx] = pMP;
				nmatches++;
			}

		}

		return nmatches;
	}

	int ORBmatcher::Fuse(KeyFrame *pKF, cv::Mat Scw, const vector<MapPoint *> &vpPoints, float th, vector<MapPoint *> &vpReplacePoint) {

		// ����ڲ�
		const float &fx = pKF->m_pCameraInfo->m_fx;
		const float &fy = pKF->m_pCameraInfo->m_fy;
		const float &cx = pKF->m_pCameraInfo->m_cx;
		const float &cy = pKF->m_pCameraInfo->m_cy;

		// ת����R��t
		cv::Mat sRcw = Scw.rowRange(0, 3).colRange(0, 3);
		const float scw = sqrt(sRcw.row(0).dot(sRcw.row(0)));
		cv::Mat Rcw = sRcw / scw;
		cv::Mat tcw = Scw.rowRange(0, 3).col(3) / scw;
		cv::Mat Ow = -Rcw.t()*tcw;

		// �Ѿ��ҵ���MapPoint����KeyFrame�����еģ���ЩMapPoint�Ͳ��ò�������ˣ�
		const set<MapPoint*> spAlreadyFound = pKF->GetMapPoints();

		int nFused = 0;

		const int nPoints = vpPoints.size();

		// For each candidate MapPoint project and match
		for (int iMP = 0; iMP < nPoints; iMP++) {

			MapPoint* pMP = vpPoints[iMP];

			// �����Ѿ��ҵ���
			if (pMP->isBad() || spAlreadyFound.count(pMP))
				continue;

			// �õ���ά�����꣬׼��ͶӰ��KeyFrame�ռ�
			cv::Mat p3Dw = pMP->GetWorldPos();

			// ת�����������ϵ
			cv::Mat p3Dc = Rcw*p3Dw + tcw;

			// depth��������������Ȼ��MapPoint����������ǿ�������
			if (p3Dc.at<float>(2) < 0.0)
				continue;

			// ͶӰ��ͼƬ�ϣ�����ͼƬ������
			const float invz = 1 / p3Dc.at<float>(2);
			const float x = p3Dc.at<float>(0)*invz;
			const float y = p3Dc.at<float>(1)*invz;

			const float u = fx*x + cx;
			const float v = fy*y + cy;

			// ͶӰ��ͼƬ�ϵ����������ͼƬ�߽緶Χ��
			if (!pKF->IsInImage(u, v))
				continue;

			// ��������ĵľ��뻹Ҫ�����MapPoint��Χ���ƣ���Ȼ�ڸ�KeyFrameҲ���Ҳ�����MapPoint��
			const float maxDistance = pMP->GetMaxDistanceInvariance();
			const float minDistance = pMP->GetMinDistanceInvariance();
			cv::Mat PO = p3Dw - Ow;
			const float dist = cv::norm(PO);

			if (dist<minDistance || dist>maxDistance)
				continue;

			// ���㵽������ĵķ���͸�MapPointƽ������н�ҪС��60�ȣ�Ҳ�����ӽ�ҪС��60��
			cv::Mat Pn = pMP->GetNormal();

			if (PO.dot(Pn) < 0.5*dist)
				continue;


			// ���Ƹõ�������ڸ�KeyFrame2Ӧ�ô��ڽ�����ģ�͵�Level
			int nPredictedLevel = pMP->PredictScale(dist, pKF->m_pPLevelInfo->m_fLogScaleFactor);

			// �ж�֮ǰ�жϵ��ǲ��Ǻ������������������ģ����ܱ�֤��õ�Scale Level�ǺϷ���
			if (nPredictedLevel >= pKF->m_pPLevelInfo->m_vScaleFactors.size())
				continue;

			// ��ǰ����ö�άͶӰ��KeyFrameĳ���뾶��Χ������ص�KeyPoint
			// LevelԽ����ζ��ͼƬԽС�������ԭͼ�Ͻ��в��ҵĻ��뾶����Ҫ������Ӧ�ķŴ�
			const float radius = th*pKF->m_pPLevelInfo->m_vScaleFactors[nPredictedLevel];

			// // �õ��÷�Χ�ڵ�KeyPoint��index�ļ���
			const vector<size_t> vIndices = pKF->GetFeaturesInArea(u, v, radius);

			if (vIndices.empty())
				continue;

			// ��ǰ��ĺܶ෽��һ���������MapPoint�������������KeyPoint,ͬʱ������һ������ֵ
			// ����Ϊ��MapPoint��KeyPoint��Ӧ��MapPointƥ�����ˣ���ȻҪ�����Ѿ�ƥ���ϵ�MapPoint
			const cv::Mat dMP = pMP->GetDescriptor();

			int bestDist = 256;
			int bestIdx = -1;
			for (vector<size_t>::const_iterator vit = vIndices.begin(), vend = vIndices.end(); vit != vend; vit++) {
				const size_t idx = *vit;
				
				const int &kpLevel = pKF->m_vKeysUn[idx].octave;

				if (kpLevel<nPredictedLevel - 1 || kpLevel>nPredictedLevel)
					continue;

				const cv::Mat &dKF = pKF->m_Descriptors.row(idx);

				const int dist = DescriptorDistance(dMP, dKF);

				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = idx;
				}
			}

			if (bestDist <= TH_LOW) {
				MapPoint* pMPinKF = pKF->GetMapPoint(bestIdx); // ��KeyFrame�к͸�MapPointƥ���ϵ�MapPoint
				if (pMPinKF) {
					if (!pMPinKF->isBad()){
						vpReplacePoint[iMP] = pMPinKF;
						//pMP->Replace(pMPinKF); // û������������������棬��Ϊ�˵�ͼ��ͬ�������������ʹ��ͼ��pMPʧЧ
					}
				}
				else {
					pMP->AddObservation(pKF, bestIdx);
					pKF->AddMapPoint(pMP, bestIdx);
				}
				nFused++;
			}
		}

		return nFused;
	}


	void ORBmatcher::ComputeThreeMaxima(vector<int>* histo, const int L, int &ind1, int &ind2, int &ind3) {
		int max1 = 0;
		int max2 = 0;
		int max3 = 0;

		for (int i = 0; i < L; i++) {
			const int s = histo[i].size();
			if (s > max1) {
				max3 = max2;
				max2 = max1;
				max1 = s;
				ind3 = ind2;
				ind2 = ind1;
				ind1 = i;
			} else if (s > max2) {
				max3 = max2;
				max2 = s;
				ind3 = ind2;
				ind2 = i;
			} else if (s > max3) {
				max3 = s;
				ind3 = i;
			}
		}

		if (max2 < 0.1f*(float)max1) {
			ind2 = -1;
			ind3 = -1;
		} else if (max3 < 0.1f*(float)max1) {
			ind3 = -1;
		}
	}


	// Bit set count operation from
	// http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	int ORBmatcher::DescriptorDistance(const cv::Mat &a, const cv::Mat &b) {

		const int *pa = a.ptr<int32_t>();
		const int *pb = b.ptr<int32_t>();

		int dist = 0;

		for (int i = 0; i < 8; i++, pa++, pb++) {
			unsigned  int v = *pa ^ *pb;
			v = v - ((v >> 1) & 0x55555555);
			v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
			dist += (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
		}

		return dist;
	}
	
	bool ORBmatcher::CheckDistEpipolarLine(const cv::KeyPoint &kp1, const cv::KeyPoint &kp2, const cv::Mat &F12, const KeyFrame* pKF2) {
		// Epipolar line in second image l = x1'F12 = [a b c]
		const float a = kp1.pt.x*F12.at<float>(0, 0) + kp1.pt.y*F12.at<float>(1, 0) + F12.at<float>(2, 0);
		const float b = kp1.pt.x*F12.at<float>(0, 1) + kp1.pt.y*F12.at<float>(1, 1) + F12.at<float>(2, 1);
		const float c = kp1.pt.x*F12.at<float>(0, 2) + kp1.pt.y*F12.at<float>(1, 2) + F12.at<float>(2, 2);

		const float num = a*kp2.pt.x + b*kp2.pt.y + c;

		const float den = a*a + b*b;

		if (den == 0)
			return false;

		const float dsqr = num*num / den;

		return dsqr < 3.84*pKF2->m_pPLevelInfo->m_vLevelSigma2[kp2.octave];  // ����ֵ����������
	}
}