/*
    Copyright 2011 Roberto Panerai Velloso.

    This file is part of libsockets.

    libsockets is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libsockets is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libsockets.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <queue>
#include <iostream>
#include <set>
#include "tTPSFilter.h"
#include "misc.h"

tTPSFilter::tTPSFilter() : count(0),pathCount(0) {
}

tTPSFilter::~tTPSFilter() {
}

const wstring& tTPSFilter::getTagPathSequence(int dr) {
	if (dr < 0)
		return tagPathSequence;
	else {
		size_t i = dr;

		if (i < regions.size())
			return regions[dr].tps;
		else
			return tagPathSequence;
	}
}

bool tTPSFilter::prune(tNode *n) {
	list<tNode *> remove;

	if (n == NULL) return false;

	for (auto i=n->nodes.begin();i!=n->nodes.end();i++) {
		if (prune(*i)) remove.push_back(*i);
	}

	for (auto i=remove.begin();i!=remove.end();i++) {
		n->size -= (*i)->size;
		n->nodes.remove(*i);
		count--;
	}

	if (find(nodeSequence.begin(),nodeSequence.end(),n) == nodeSequence.end() &&
		n->nodes.empty()) {
		return true;
	}
	return false;
}

long int tTPSFilter::searchRegion(wstring s) {
	set<int> alphabet,filteredAlphabet,regionAlphabet,intersect;
	map<int,int> currentSymbolCount,symbolCount,thresholds;
	bool regionFound=false;
	size_t border=0;


	// compute symbol frequency
	for (size_t i=0;i<s.size();i++) {
		if (alphabet.find(s[i]) == alphabet.end()) {
			symbolCount[s[i]]=0;
			alphabet.insert(s[i]);
		}
		symbolCount[s[i]]++;
	}

	// create sorted list of frequency thresholds
	for (auto i=symbolCount.begin();i!=symbolCount.end();i++)
		thresholds[(*i).second] = (*i).first;
	auto threshold = thresholds.begin();

	while (!regionFound && (threshold != thresholds.end())) {
		// filter alphabet
		filteredAlphabet.clear();
		for (auto j=symbolCount.begin();j!=symbolCount.end();j++) {
			if ((*j).second > (*threshold).first) filteredAlphabet.insert((*j).first);
		}
		if (filteredAlphabet.size() < 2) break;
		threshold++;

		regionAlphabet.clear();
		currentSymbolCount = symbolCount;
		for (size_t i=0;i<s.size();i++) {
			if (filteredAlphabet.find(s[i]) != filteredAlphabet.end()) {
				regionAlphabet.insert(s[i]);
				currentSymbolCount[s[i]]--;
				if (currentSymbolCount[s[i]]==0) {
					filteredAlphabet.erase(s[i]);
					set_intersection(filteredAlphabet.begin(),filteredAlphabet.end(),regionAlphabet.begin(),regionAlphabet.end(),inserter(intersect,intersect.begin()));

					if (intersect.empty()) {
						if (!filteredAlphabet.empty()) {
							regionFound = true;
							border=i;
						}
						break;
					}
					intersect.clear();
				}
			}
		}
	}

	if (regionFound) {
		if (border <= s.size()/2) {
			border++;
			s = s.substr(border,s.size());
		} else {
			s = s.substr(0,border);
			border = 0;
		}
		tagPathSequence = s;
		border += searchRegion(s);
	}
	return border;
}

void tTPSFilter::buildTagPath(string s, tNode *n, bool print, bool css, bool fp) {
	auto i = n->nodes.begin();
	string tagStyle,tagClass;

	if (s == "") {
		pathCount = 0;
		tagPathMap.clear();
		tagPathSequence.clear();
		nodeSequence.clear();
	}

	tagStyle = n->getAttribute("style");
	tagClass = n->getAttribute("class");
	if (tagStyle != "") tagStyle = " " + tagStyle;
	if (tagClass != "") tagClass = " " + tagClass;
	if (css) s = s + "/" + n->getTagName() + tagClass + tagStyle;
	else s = s + "/" + n->getTagName();

	if (tagPathMap.find(s) == tagPathMap.end()) {
		pathCount++;
		tagPathMap[s] = pathCount;
	}
	tagPathSequence = tagPathSequence + wchar_t(tagPathMap[s]);
	nodeSequence.push_back(n);

	if (print) {
		cout << tagPathMap[s];
		if (fp) cout << ":\t" << s;
		cout << endl;
	}

	if (!(n->nodes.size())) return;

	for (;i!=n->nodes.end();i++)
		buildTagPath(s,*i,print,css,fp);
}

map<long int, tTPSRegion> tTPSFilter::tagPathSequenceFilter(tNode *n, bool css) {
	wstring originalTPS;
	vector<tNode *> originalNodeSequence;
	queue<pair<wstring,long int>> seqQueue;
	vector<long int> start;
	map<long int,tTPSRegion> structured;
	int originalTPSsize;
	long int sizeThreshold;

	buildTagPath("",n,false,css,false);
	originalTPS = tagPathSequence;
	originalNodeSequence = nodeSequence;
	originalTPSsize = originalTPS.size();
	sizeThreshold = (originalTPSsize*5)/100; // % page size

	seqQueue.push(make_pair(originalTPS,0)); // insert first sequence in queue for processing

	while (seqQueue.size()) {
		long int len,off,rlen,pos;
		wstring tps;

		auto s = seqQueue.front();

		seqQueue.pop();

		tps = s.first;
		len = tps.size();
		off = s.second;
		pos = searchRegion(tps);
		rlen = tagPathSequence.size();

		if (len > rlen) {
			if (pos > 0)
				seqQueue.push(make_pair(tps.substr(0,pos),off));
			if ((len-pos-rlen) > 0)
				seqQueue.push(make_pair(tps.substr(pos+rlen),off+pos+rlen));
			if (rlen > sizeThreshold) {
				_regions[off+pos].len=rlen;
				_regions[off+pos].tps = originalTPS.substr(off+pos,rlen);
			}
		}
	}

	if (_regions.size()) {
		auto r=_regions.begin();
		if ((*r).first > sizeThreshold) {
			_regions[0].len = (*r).first;
			_regions[0].tps = originalTPS.substr(0,(*r).first);
		}
	} else {
		_regions[0].len=originalTPSsize;
		_regions[0].tps = originalTPS;
	}
	// select structured regions
	float angCoeffThreshold=0.1;

	for (auto i=_regions.begin();i!=_regions.end();i++) {
		tLinearCoeff lc;
		lc = linearRegression((*i).second.tps);
		(*i).second.lc.a = lc.a;
		(*i).second.lc.b = lc.b;
		(*i).second.lc.e = lc.e;

		cerr << "size: " << (*i).second.len << " ang.coeff.: " << (*i).second.lc.a << endl;

		if (abs((*i).second.lc.a) < angCoeffThreshold)
			structured.insert(*i);
	}

	tagPathSequence = originalTPS;
	nodeSequence = originalNodeSequence;

	return structured;
}

void tTPSFilter::DRDE(tNode *n, bool css, float st) {
	vector<unsigned int> recpos;
	vector<wstring> m;
	map<long int, tTPSRegion> structured;

	_regions.clear();

	structured=tagPathSequenceFilter(n,css); // locate main content regions

	for (auto i=structured.begin();i!=structured.end();i++) {
		auto firstNode = nodeSequence.begin()+(*i).first;
		auto lastNode = firstNode + (*i).second.len;

		_regions[(*i).first].tps = tagPathSequence.substr((*i).first,(*i).second.len);
		_regions[(*i).first].nodeSeq.assign(firstNode,lastNode);
		m.clear();
		recpos.clear();

		cerr << "TPS: " << endl;
		for (size_t j=0;j<_regions[(*i).first].tps.size();j++)
			cerr << _regions[(*i).first].tps[j] << " ";
		cerr << endl;

		// identify the start position of each record
		recpos = locateRecords(_regions[(*i).first].tps,st);

		// consider only leaf nodes when searching record boundary & performing field alignment
		/*for (size_t k=0,j=0;j<_regions[(*i).first].nodeSeq.size();j++,k++) {
			if (_regions[(*i).first].nodeSeq[j]->nodes.size() || _regions[(*i).first].nodeSeq[j]->text=="") {
				_regions[(*i).first].nodeSeq.erase(_regions[(*i).first].nodeSeq.begin()+k);
				_regions[(*i).first].tps.erase(k,1);
				for (size_t w=0;w<recpos.size();w++) {
					if (recpos[w]>k) recpos[w]--;
				}
				k--;
			}
		}*/

		// create a sequence for each record found
		int prev=-1;
		unsigned int max_size=0;
		for (size_t j=0;j<recpos.size();j++) {
			if (prev==-1) prev=recpos[j];
			else {
				m.push_back(_regions[(*i).first].tps.substr(prev,recpos[j]-prev));
				max_size = max(recpos[j]-prev,max_size);
				prev = recpos[j];
			}
		}
		if (prev != -1) {
			if (max_size == 0) max_size = recpos.size();
			m.push_back(_regions[(*i).first].tps.substr(prev,/*_regions[(*i).first].tps.size()-prev+1*/max_size));
		}

		// align the records (one alternative to 'center star' algorithm is ClustalW)
		centerStar(m);

		// and extracts them
		if (m.size()) onDataRecordFound(m,recpos,&_regions[(*i).first]);
	}

	regions.clear();
	for (auto i=_regions.begin();i!=_regions.end();i++) {
		(*i).second.pos = (*i).first;
		regions.push_back((*i).second);
	}
}

vector<unsigned int> tTPSFilter::locateRecords(wstring s, float st) {
	vector<int> d(s.size()-1);
	map<int, vector<int> > diffMap;
	map<int, int> TPMap;
	vector<unsigned int> recpos;
	int rootTag;
	int tagCount=0;
	int gap=0;
	size_t interval=0xffffffff;

	/* compute sequence's first difference, keeping only the negative values (i.e. keeping only
	 * fast transitions from very high to very low values, L - H = negative difference). The
	 * difference points are stored and processed in ascending order (higher absolute values first).
	 *
	 * the difference is weighted with the inverse TPC value, the lower, the better
	*/


	auto z=s;
	auto off=trimSequence(z);

	cerr << off << " diff: " << endl;

	for (size_t i=1;i<z.size();i++) {
		d[i-1]=(z[i]-z[i-1])*z[i-1];
		/*if (d[i-1] < 0) {
			cerr << d[i-1]*s[i] << " ";
			diffMap[d[i-1]].push_back(i);
		} else cerr << 0 << " ";*/
	}

	for (size_t i=1;i<d.size();i++) {
		if (sign(d[i-1])==sign(sign(d[i]))) {
			if (sign(d[i])>0)
				d[i] += d[i-1];
			else
				d[i] -= d[i-1];
		} else if (d[i] >= 0) {
			diffMap[d[i-1]].push_back(i+off);
		}
		cerr << (d[i-1]<0?d[i-1]:0) << " ";
	}
	cerr << endl;

	// process lowest values until the gap between points achieve enough sequence coverage
	int l=(*(diffMap.begin())).second[0];
	int r=l;
	for (auto i=diffMap.begin();i!=diffMap.end();i++) {

		for (size_t j=0; j<(*i).second.size();j++) {
			if ((*i).second[j]<l) l = (*i).second[j];
			if ((*i).second[j]>r) r = (*i).second[j];
			TPMap[s[(*i).second[j]]]++;
			cerr << "TPS[" << (*i).second[j] << "] = " << s[(*i).second[j]] << endl;
			if (j>1) {
				size_t itv = abs((*i).second[j]-(*i).second[j-1]);
				if (itv < interval) interval = itv;
			}
		}
		if (interval!=0xffffffff) gap += interval*(*i).second.size();
		if (((float)gap / (float)s.size()) > st) break;
	}

	// find the most frequent tag path code within the lowest difference values
	for (auto i=TPMap.begin();i!=TPMap.end();i++) {
		cerr << (*i).first << " " << (*i).second << endl;
		if ((*i).second > tagCount) {
			tagCount = (*i).second;
			rootTag = (*i).first;
		}
	}

	/* TESTE */
	TPMap.clear();
	float _max_score=0;
	for (size_t i=0;i<=s.size();i++) {
		TPMap[s[i]]++;
	}
	rootTag=0;
	for (auto i=TPMap.begin();i!=TPMap.end();i++) {
		if ((*i).first <= 0) continue;
		if ((*i).second < (s.size()*0.01)) continue;
		float _score = (((float)((*i).second)) / ((float)((*i).first)));
		cerr << "*** SCORE " << (*i).first << " / " << (*i).second << " = " << _score << endl;
		if ( _score >= _max_score) {
			if ((rootTag > (*i).first) || (!rootTag)) {
				tagCount = (*i).second;
				rootTag = (*i).first;
				_max_score = _score;
			}
		}
	}
	cerr << "*** FINAL " << rootTag << " / " << tagCount << " = " << _max_score << endl;
	/* fim TESTE */

	// find the beginning of each record, using the tag path code found before
	for (size_t i=0;i<s.size();i++) {
		if (s[i] == rootTag) {
			//cerr << "root: " << i << " " << nodeSequence[i]->tagName << " : " << nodeSequence[i]->text << endl;
			recpos.push_back(i);
		}
	}

	return recpos;
}

tTPSRegion *tTPSFilter::getRegion(size_t r) {
	if (r < regions.size())
		return &regions[r];
	return NULL;
}

size_t tTPSFilter::getRegionCount() {
	return regions.size();
}

vector<tNode*> tTPSFilter::getRecord(size_t dr, size_t rec) {
	if (dr < regions.size()) {
		if (rec < regions[dr].records.size())
			return regions[dr].records[rec];
	}
	return vector<tNode *>(0);
}

void tTPSFilter::onDataRecordFound(vector<wstring> &m, vector<unsigned int> &recpos, tTPSRegion *reg) {
	if ((m.size() == 0) || (recpos.size() == 0)) return;// -1;

	int rows=m.size(),cols=m[0].size();
	vector<tNode *> rec;

	for (int i=0;i<rows;i++) {
		rec.clear();
		for (int j=0,k=0;j<cols;j++) {
			if (m[i][j] != 0) {
				rec.push_back(reg->nodeSeq[recpos[i]+k]);
				k++;
			} else rec.push_back(NULL);
		}
		reg->records.push_back(rec);
	}
	cleanRegion(reg->records);
}
