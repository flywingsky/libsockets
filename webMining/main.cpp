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

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <cmath>
#include <cerrno>
#include "tnode.h"
#include "tdom.h"
#include "misc.h"


using namespace std;

class tFormExtractDOM : public tDOM {
public:
	tFormExtractDOM() : tDOM() {};
	//virtual ~tCustomDOM() {};
	virtual void onTagFound(tNode *n) {
		if (n->getTagName() == "form") {
			cout << n->getTagName()
			<< ";" << n->getAttribute("action")
			<< ";" << n->getAttribute("method")
			<< ";" << n->getAttribute("name")
			<< endl;
			cout << "tag;type;value;name;size" << endl;
			searchForm(n);
		}
		cout << endl;
	}
	virtual void onPatternFound(tNode *n, tNode *p, float s) {};
	virtual void onDataRecordFound(tDataRegion dr) {};
	virtual void onDataRecordFound(vector<wstring> &m, vector<unsigned int> &recpos) {};
protected:
	void searchForm(tNode *n) {
		auto i = n->getNodes().begin();

		for (;i!=n->getNodes().end();i++) {
			if (((*i)->getTagName() == "input") ||
				((*i)->getTagName() == "select") ||
				((*i)->getTagName() == "option") ||
				((*i)->getTagName() == "textarea"))
				cout << (*i)->getTagName()
				<< ";" << (*i)->getAttribute("type")
				<< ";" << (*i)->getAttribute("value")
				<< ";" << (*i)->getAttribute("name")
				<< ";" << (*i)->getAttribute("size")
				<< endl;
			searchForm(*i);
		}
	}
};

class tCustomDOM : public tDOM {
public:
	tCustomDOM() : tDOM() {
		g = r = xml = maxScore = maxCount = maxCount2 = recCountDisplay = 0;
		filterStr = "";
		filterTag = "";
	};
	virtual ~tCustomDOM() {
		if (!recCountDisplay) {
			if (r) {
				if (scores.size()) {
					if (xml) cout << "<scores>";
					else cout << "<br><div id='scores'>";

					for (auto i=scores.begin();i!=scores.end();i++) {
						cout << (*i) << ";" << 100.00*(float)(*i)/(float)maxScore << "%";
						if (!xml) cout << "<br>";
						cout << endl;
					}

					if (xml) cout << "</scores>";
					else cout << "</div>";
					cout << endl;

					scores.clear();
				}
				if (xml) cout << "<region-count>" << g << "</region-count>" << endl << "<record-count>" << r << "</record-count>" << endl << "</extraction>" << endl;
				else cout << "<h3>Found " << r << " result(s) in " << g << " region(s).</h3></html>" << endl;
			}
		} else {
			cout << maxCount;
			if (maxCount2) cout << " " << maxCount2;
			cout << endl;
		}
		exit(0);
	};
	virtual void onTagFound(tNode *n) {
		r++;
		if (verbose) {
			printNode(n,1);
			cout << endl;
		} else {
			switch (n->getType()) {
			case 0:
				cout << "<" << n->getTagName();
				if (n->getText().size()>0) cout << " " << n->getText();
				cout << ">" <<endl;
				break;
			case 1:
				cout << "</" << n->getTagName();
				if (n->getText().size()>0) cout << " " << n->getText();
				cout << ">" << endl;
				break;
			case 2:
			case 3:
				cout << n->getTagName();
				if (n->getText().size()>0) cout << " " << n->getText();
				cout << endl;
				break;
			default:
				break;
			}
		}
	};

	virtual void onPatternFound(tNode *n, tNode *p, float s) {
		cout << "<DIV class=\"pattern\"> " << ++r << " Similarity " << (s*100.0) << "%" << endl;
		printNode(n,1);
		cout << "</DIV>" << endl << endl;
	};

	virtual void onDataRecordFound(tDataRegion dr) {
		vector<tNode *> recs = partialTreeAlignment(dr);
		list<tNode *> alignments[recs.size()];
		size_t reccount=0,recsize=0;

		for (size_t i=0;i<recs.size();i++) {
			alignments[i] = getRecord(recs[0],recs[i]);
			reccount += (alignments[i].size()>0);
			recsize = max(recsize,alignments[i].size());
		}

		if (reccount > 0) {
			int gr=0,score=reccount*recsize;

			maxScore = max(maxScore,score);
			if (maxScore == score) {
				maxCount2 = maxCount;
				maxCount = reccount;
			}
			scores.push_back(score);

			if (!recCountDisplay) {
				if (!g++) {
					if (xml)
						cout << "<?xml version=\"1.0\"""?>" << endl << "<extraction>" << endl;
					else
						cout << "<html>" << endl;
				}

				if (xml) {
					cout << " <region number=\"" << g << "\" recsize=\"" << recsize << "\" reccount=\"" << reccount << "\" score=\"" << recsize*reccount << "\">" << endl;
					cout << "  <regex><![CDATA[" + getRegEx(recs[0],reccount) + "]]></regex>" << endl;
				} else {
					cout << "<b><div>Region <span>" << g << "</span> / Rec. size  <span>" << recsize << "</span> / Rec. Count <span>" << reccount << "</span> / Score <span>" << recsize*reccount << "</span></div></b><br>" << endl << "<table border=1>" << endl;
					cout << "<tr><td>RegEx</td><td colspan=" << recsize << "><textarea>" << getRegEx(recs[0],reccount) << "</textarea></td></tr>" << endl;
				}

				for (size_t i=0;i<recs.size();i++) {
					list<tNode *> fields = alignments[i];

					if (alignments[i].size() > 0) {
						r++; gr++;

						if (xml)
							cout << "  <record number=\"" << gr << "\">" << endl;
						else
							cout << "<tr><th>" << gr << "</th>";

						for (auto j=fields.begin();j!=fields.end();j++) {
							if (xml) {
								cout << "   <field tag=\"";
								if ((*j)) {
									cout << (*j)->tagName << "\"><![CDATA[";
									printNode(*j,4);
									cout << "]]>";
								} else cout << "\">";
								cout << "</field>" << endl;
							} else {
								cout << "<td>";
								if ((*j)) printNode(*j,1);
								cout << "</td>";
							}
						}
						if (xml)
							cout << "  </record>" << endl;
						else
							cout << "</tr>" << endl;
					}
				}

				if (xml)
					cout << " </region>" << endl;
				else
					cout << "</table><br>" << endl;
			}
		}
	};

	virtual void onDataRecordFound(vector<wstring> &m, vector<unsigned int> &recpos) {
		if ((m.size() == 0) || (recpos.size() == 0)) return;

		size_t rows=m.size(),cols=m[0].size();
		vector<vector<tNode *> > table(rows, vector<tNode *>(cols));
		bool printField[cols];
		size_t recsize = cols;

		for (size_t i=0;i<rows;i++) {
			for (size_t j=0,k=0;j<cols;j++) {
				if (m[i][j] != 0) {
					table[i][j] = nodeSequence[recpos[i]+k];
					k++;
				} else table[i][j]=NULL;
			}
		}

		for (size_t j=0;j<cols;j++) {
			printField[j]=false;
			for (size_t i=0;i<rows;i++) {
				if (table[i][j]) {
					if ((table[i][j]->tagName == "a") ||
						(table[i][j]->tagName == "img") ||
						((table[i][j]->type == 2) && (table[i][j]->text != ""))
						) {
						printField[j]=true;
						break;
					}
				}
			}
			if (!printField[j]) recsize--;
		}

		if (xml) {
			cout << "<?xml version=\"1.0\"""?>" << endl << "<extraction>" << endl;
			cout << " <region number=\"" << 1 << "\" recsize=\"" << recsize << "\" reccount=\"" << m.size() << "\" score=\"" << recsize*r << "\">" << endl;
		} else {
			cout << "<meta http-equiv=""Content-Type"" content=""text/html;charset=utf-8"">" << endl;
			cout << "<table border=1>" << endl;
			cout << "<tr><th>#</th><th>Record size:" << recsize << "</th><th>Record count: " << rows << "</th><th colspan=" << m[0].size() - 2 << "></th>";
		}

		for (size_t i=0;i<rows;i++) {

			if (xml) {
				cout << "  <record number=\"" << i+1 << "\">" << endl;
			} else
				cout << "<tr><th> #" << i+1 << "</th>";

			for (size_t j=0;j<cols;j++) {

				if (!printField[j]) {
					continue;
				}

				if (xml) {
					cout << "   <field><![CDATA[";
				} else
					cout << "<td>";

				if (table[i][j]) {
					tNode *n = table[i][j];

					if ((n->tagName == "img") || (n->tagName == "a")) {
						printNode(n,4);
					} else if (n->type == 2) {
						cout << n->text;
					/*} else if (n->nodes.size() == 0) {
						cout << n->tagName << " " << n->text;*/
					}
				}

				if (xml) {
					cout << "]]>";
					cout << "</field>" << endl;
				} else
					cout << "</td>";
			}
			if (xml)
				cout << "  </record>" << endl;
			else
				cout << "</tr>";
		}
		if (xml)
			cout << " </region>" << endl;
		else
			cout << "</table>" << endl;

		g++;
		r+=rows;
	}

	int filter(tNode *n) {
		int ret=1;

		if ((filterStr != "") || (filterTag != "")) {
			if (filterTag == "") filterTag = "#text";
			ret = searchString(n,filterTag,filterStr,0);
		}
		return ret && n->depth>2;
	};

	int g,r,xml,maxScore,maxCount,maxCount2,recCountDisplay;
	string filterStr,filterTag;
	list<int> scores;
};

void printUsage(char *p)
{
	cout << "usage: "<<p<<" [-i input_file] [-o output_file] [-p pattern file] [-s search_string] [-v] [-t ###.##] [-m] [-xml] [-f str] [-a] [-b] [-g] [-c] [-r] [-q]"<<endl;
	cout << "-i    input file (default stdin)"<<endl;
	cout << "-o    output file (default stdout)"<<endl;
	cout << "-p    pattern file to search for"<<endl;
	cout << "-s    search string: a list of tags to search for (tag1,tag2,...)"<<endl;
	cout << "-t    value. Similarity threshold. default 100%. Ex.: -t 90.7 (90.7%)" << endl;
	cout << "-v    Verbose (do not abbreviate tags/text content." << endl;
	cout << "-m    performs MDR" << endl;
	cout << "-xml  outputs MDR results in XML format" << endl;
	cout << "-f    text filter string" << endl;
	cout << "-a[p] display tag path of input (-ap prints full path)" << endl;
	cout << "-b[p] display tag path of input without style" << endl;
	cout << "-g    mine forms and fields" << endl;
	cout << "-c    displays only record count of main region." << endl;
	cout << "-r    apply tag path sequence filter." << endl;
	cout << "-q    DRDE (difference record detection and extraction)." << endl;
	cout << "-z    LZ extraction." << endl;
	exit(-1);
}

#define K 10

int main(int argc, char *argv[])
{
	int opt,mdr=0,tp=0,mineForms=0,dbg=0,tpsFilter=0,lz=0;
	bool style=true,DRE=false,tpcfp=false;
	float st=1.0; // similarity threshold
	string inp="",outp="",search="",pattern="",filterStr="";
	tCustomDOM *d = new tCustomDOM();
	tCustomDOM *p = new tCustomDOM();
	tFormExtractDOM *fe = new tFormExtractDOM();
	fstream patternFile,inputFile;
	filebuf outputFile;

	while ((opt = getopt(argc, argv, "i:o:t:s:p:f:x:a::b::d:mhvcgrqz")) != -1) {
		switch (opt) {
		case 'c':
			d->recCountDisplay = 1;
			break;
		case 'i':
			inp = optarg;
			break;
		case 'o':
			outp = optarg;
			break;
		case 's':
			search = optarg;
			break;
		case 'p':
			pattern = optarg;
			break;
		case 't':
			st = atof(optarg)/100;
			break;
		case 'v':
			d->setVerbose(1);
			break;
		case 'm':
			mdr = 1;
			break;
		case 'r':
			tpsFilter = 1;
			break;
		case 'f':
			filterStr = optarg;
			break;
		case 'a':
		case 'b':
			tp = 1;
			style = (opt == 'a');
			if (optarg) tpcfp = (string(optarg) == "p");
			break;
		case 'x':
			if (optarg && string(optarg) == "ml") d->xml=1;
			else printUsage(argv[0]);
			break;
		case 'g':
			mineForms = 1;
			break;
		case 'q':
			DRE = true;
			break;
		case 'z':
			lz = 1;
			break;
		case 'd':
			if (optarg && string(optarg) == "ebug") dbg=1;
			else  printUsage(argv[0]);
			break;
		case 'h':
		default:
	    	printUsage(argv[0]);
	    break;
		}
	}

	if (!dbg) fclose(stderr);

	if (pattern != "") {
		patternFile.open(pattern.c_str());
		if (patternFile.is_open()) {
			p->scan(patternFile);
			patternFile.close();
		} else {
			perror("open");
			return -1;
		}
	}

	if (inp != "") {
		inputFile.open(inp.c_str());
		if (inputFile.is_open()) {
			d->scan(inputFile);
			inputFile.close();
		} else {
			perror("open");
			return -1;
		}
	} else {
		d->scan(cin);
	}

	if (outp != "") {
		outputFile.open(outp.c_str(),ios_base::out|ios_base::binary);

		if (!errno) cout.rdbuf(&outputFile);
	}

	if (tpsFilter) {
		d->tagPathSequenceFilter();
	}

	if (mineForms) {
		fe->setRoot(d->getRoot());
		fe->searchTag("form","");
		return 0;
	}

	if (tp) {
		d->buildTagPath("",d->getBody(),true,style,tpcfp);
	}

	if (lz) {
		d->buildTagPath("",d->getBody(),false,true);
		d->LZExtraction();
	}

	if (!tp && !mdr && search != "") {
		string t,s=search;

		while ((t=stringTok(s,","))!="") {
			lowerCase(t);
			d->searchTag(t,filterStr);
		}
	}

	if (!tp && !mdr && pattern != "") {
		d->searchPattern(p,st);
	}

	if (mdr) {
		d->filterTag = search;
		d->filterStr = filterStr;
		d->MDR(d->getRoot(),K,st,1);
		cerr << "Similarity threshold used: " << st << endl;
	}

	if (DRE) {
		d->DRDE(st);
	}

	if (search == "" && pattern == "" && !mdr && !tp && !lz && !DRE) {
		d->printDOM();
	}

	delete d;
	delete p;

	cout.flush();
	return 0;
}
