#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <map>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <openssl/sha.h>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace fs=std::filesystem;
using namespace std;
string getCurrentTimestamp() 
{
    auto now=chrono::system_clock::now();
    time_t now_c=chrono::system_clock::to_time_t(now);
    tm *parts=localtime(&now_c);
    ostringstream oss;
    oss<<std::put_time(parts,"%Y-%m-%d %H:%M:%S");
    return oss.str();
}
vector<string> splitLines(const string &str) 
{
    vector<string>lines;
    istringstream iss(str);
    string line;
    while(getline(iss,line))
    lines.push_back(line);
    return lines;
}
string joinLines(const vector<string>& lines) 
{
    ostringstream oss;
    for(const auto &line:lines)
    oss<<line<<"\n";
    return oss.str();
}
string computeDiff(const string &oldContent,const string &newContent) 
{
    vector<string> oldLines=splitLines(oldContent);
    vector<string> newLines=splitLines(newContent);
    int m=oldLines.size(),n=newLines.size();
    vector<vector<int>>lcs(m + 1,vector<int>(n+1,0));
    for(int i=1;i<=m;i++)
    for(int j=1;j<=n;j++) 
    if(oldLines[i-1]==newLines[j-1])
    lcs[i][j]=lcs[i-1][j-1]+1;
    else
    lcs[i][j]=max(lcs[i-1][j],lcs[i][j-1]);
    int i=m,j=n;
    vector<string>rawDiff;
    while(i>0&&j>0) 
    {
        if(oldLines[i-1]==newLines[j-1]) 
        {
            rawDiff.push_back(" "+oldLines[i-1]);
            i--; 
            j--;
        } 
        else if(lcs[i-1][j]>=lcs[i][j-1]) 
        {
            rawDiff.push_back("-"+oldLines[i-1]);
            i--;
        } 
        else 
        {
            rawDiff.push_back("+"+newLines[j-1]);
            j--;
        }
    }
    while(i>0) 
    {
        rawDiff.push_back("-"+oldLines[i-1]);
        i--;
    }
    while(j>0) 
    {
        rawDiff.push_back("+"+newLines[j-1]);
        j--;
    }
    reverse(rawDiff.begin(),rawDiff.end());
    vector<string>diffOutput;
    const int context=3;
    int oldIndex=0,newIndex=0;
    size_t diffIndex=0;
    int prevHunkOldEnd=0;
    while(diffIndex<rawDiff.size()) 
    {
        if(rawDiff[diffIndex][0]==' ') 
        {
            oldIndex++; 
            newIndex++;
            diffIndex++;
            continue;
        }
        int hunkOldStart=max(oldIndex-context,prevHunkOldEnd);
        int hunkNewStart=max(newIndex-context,0);
        vector<string>hunk;
        for(int k=hunkOldStart;k<oldIndex;k++)
        hunk.push_back(" "+oldLines[k]);
        while(diffIndex<rawDiff.size()) 
        {
            if(diffIndex<rawDiff.size()&&rawDiff[diffIndex][0]==' ') 
            {
                int tempI=oldIndex,tempJ=newIndex,unchanged=0;
                while(diffIndex<rawDiff.size()&&rawDiff[diffIndex][0]==' '&&unchanged<context) 
                {
                    unchanged++;
                    tempI++;
                    tempJ++;
                    diffIndex++;
                }
                if(unchanged==context) 
                {
                    for(int cnt=0;cnt<unchanged;cnt++) 
                    hunk.push_back(" "+oldLines[oldIndex+cnt]);
                    oldIndex=tempI;
                    newIndex=tempJ;
                    break;
                }
                else
                {
                    for(int cnt=0;cnt<unchanged;cnt++)
                    {
                        hunk.push_back(" "+oldLines[oldIndex]);
                        oldIndex++;
                        newIndex++;
                    }
                    break;
                }
            }
            if(diffIndex<rawDiff.size()&&rawDiff[diffIndex][0]=='-') 
            {
                hunk.push_back(rawDiff[diffIndex]);
                oldIndex++;
                diffIndex++;
                continue;
            }
            if(diffIndex<rawDiff.size()&&rawDiff[diffIndex][0]=='+') 
            {
                hunk.push_back(rawDiff[diffIndex]);
                newIndex++;
                diffIndex++;
                continue;
            }
        }
        int contextEnd=0;
        if(diffIndex>0&&(rawDiff[diffIndex-1][0]=='-'||rawDiff[diffIndex-1][0]=='+')) 
        {
            while(oldIndex+contextEnd<m&&contextEnd<context) 
            {
                hunk.push_back(" "+oldLines[oldIndex+contextEnd]);
                contextEnd++;
            }
        }
        prevHunkOldEnd=oldIndex;
        int origStart=hunkOldStart+1;
        int origCount=(oldIndex+contextEnd)-hunkOldStart;
        int newStart=hunkNewStart+1;
        int newCount=(newIndex+contextEnd)-hunkNewStart;
        ostringstream header;
        header<<"@@ -"<<origStart<<","<<origCount<<" +"<<newStart<<","<<newCount<<" @@";
        diffOutput.push_back(header.str());
        diffOutput.insert(diffOutput.end(),hunk.begin(),hunk.end());
    }
    return joinLines(diffOutput);
}
string applyDiff(const string &originalContent,const string &diffContent) {
    vector<string>origLines=splitLines(originalContent);
    vector<string>diffLines=splitLines(diffContent);
    vector<string>result;
    int currentOrigIndex=0;
    size_t diffIndex=0;
    while(diffIndex<diffLines.size()) 
    {
        string line=diffLines[diffIndex];
        if(line.rfind("@@",0)==0) 
        {
            int origStart,origCount,newStart,newCount;
            size_t pos1=line.find("-");
            size_t pos2=line.find(",",pos1);
            origStart=stoi(line.substr(pos1+1,pos2-pos1-1));
            size_t pos3=line.find(" ",pos2);
            origCount=stoi(line.substr(pos2+1,pos3-pos2-1));
            size_t pos4=line.find("+",pos3);
            size_t pos5=line.find(",",pos4);
            newStart=stoi(line.substr(pos4+1,pos5-pos4-1));
            size_t pos6=line.find(" ",pos5);
            newCount=stoi(line.substr(pos5+1,pos6-pos5-1));
            while(currentOrigIndex<origStart-1&&currentOrigIndex<(int)origLines.size()) 
            {
                result.push_back(origLines[currentOrigIndex]);
                currentOrigIndex++;
            }
            diffIndex++;
            while(diffIndex<diffLines.size()&&diffLines[diffIndex].rfind("@@", 0)!=0) 
            {
                string hunkLine=diffLines[diffIndex];
                if(hunkLine.empty()) 
                {
                    diffIndex++;
                    continue;
                }
                char prefix=hunkLine[0];
                string content=hunkLine.substr(1);
                if(prefix==' ') 
                {
                    if(currentOrigIndex<(int)origLines.size()) 
                    {
                        result.push_back(origLines[currentOrigIndex]);
                        currentOrigIndex++;
                    } 
                    else
                    result.push_back(content);
                } 
                else if (prefix=='-')
                currentOrigIndex++;
                else if (prefix == '+')
                result.push_back(content);
                diffIndex++;
            }
        } 
        else
        diffIndex++;
    }
    while(currentOrigIndex<(int)origLines.size())
    result.push_back(origLines[currentOrigIndex++]);
    return joinLines(result);
}
string generateHash(const string &input) 
{
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()),input.size(),hash);
    ostringstream oss;
    for(int i=0;i<SHA_DIGEST_LENGTH;i++)
    oss<<hex<<(int)hash[i];
    return oss.str();
}
struct GraphNode 
{
    string commitID;
    string parentID;
    string message;
    string timestamp;
    string diffFile;
};
void updateGraph(const fs::path &graphPath,const GraphNode &node) 
{
    ofstream graphFile(graphPath,ios::app);
    if(!graphFile) 
    {
        cerr<< "Error updating graph file: "<<graphPath<< "\n";
        return;
    }
    graphFile<<node.commitID<<","<<node.parentID<<","<<node.message<<","<<node.timestamp<<","<<node.diffFile<<"\n";
}
bool dfs(const string &current,const string &target,const map<string,vector<string>>&adjList,vector<string>&path) 
{
    path.push_back(current);
    if(current==target)
    return true;
    auto it=adjList.find(current);
    if(it!=adjList.end())
    for(const string &child:it->second)
    if(dfs(child,target,adjList,path))
    return true;
    path.pop_back();
    return false;
}
string reconstructFile(const fs::path &fileFolder,const string &targetCommitID) 
{
    fs::path graphPath=fileFolder/"graph.txt";
    if(!fs::exists(graphPath))
    return "";
    ifstream graphFile(graphPath);
    vector<GraphNode>nodes;
    string line;
    while(getline(graphFile,line)) 
    {
        if(line.empty()) 
        continue;
        istringstream iss(line);
        GraphNode node;
        getline(iss,node.commitID,',');
        getline(iss,node.parentID,',');
        getline(iss,node.message,',');
        getline(iss,node.timestamp,',');
        getline(iss,node.diffFile,',');
        nodes.push_back(node);
    }
    map<string,vector<string>>adjList; 
    map<string,GraphNode>meta;
    string rootCommitID="";
    for(const auto &n:nodes) 
    {
        meta[n.commitID]=n;
        if(n.parentID=="NULL")
        rootCommitID=n.commitID;
        else
        adjList[n.parentID].push_back(n.commitID);
    }
    if(rootCommitID.empty()) 
    {
        cerr<<"No root commit found (commit with parent \"NULL\").\n";
        return "";
    }
    vector<string>commitChain;
    bool found=dfs(rootCommitID,targetCommitID,adjList,commitChain);
    if(!found) 
    {
        cerr<<"Commit ID not found in graph.\n";
        return "";
    }
    string content;
    fs::path rootPath=fileFolder/"root.txt";
    ifstream rootFile(rootPath);
    if(!rootFile) 
    {
        cerr<<"Root file missing for file: "<<fileFolder.filename()<<"\n";
        return "";
    }
    ostringstream oss;
    oss<<rootFile.rdbuf();
    content=oss.str();
    for(size_t i=1;i<commitChain.size();i++) 
    {
        string commitID=commitChain[i];
        if(meta.find(commitID)==meta.end()) 
        {
            cerr<<"Metadata for commit "<<commitID<<" not found.\n";
            continue;
        }
        GraphNode node=meta[commitID];
        fs::path diffPath=fileFolder/(node.diffFile);
        ifstream diffFile(diffPath);
        if(!diffFile) 
        {
            cerr<<"Missing diff file: "<<diffPath<<"\n";
            continue;
        }
        ostringstream diffStream;
        diffStream<<diffFile.rdbuf();
        string diffContent=diffStream.str();
        content=applyDiff(content,diffContent);
    }
    return content;
}
struct VersionControl 
{
    fs::path vcsDir=".vcs";
    fs::path configFile=vcsDir/"config.txt"; 
    fs::path metadataFile=vcsDir/"vcs_meta.txt";        
    fs::path stagingDir=vcsDir/"staging_area";           
    fs::path commitsDir=vcsDir/"commits";  
    string getActiveBranch() 
    {
        ifstream config(configFile);
        if(!config) 
        return "main";
        string line,active;
        while(getline(config,line)) 
        {
            if(line.find("active_branch:")==0) 
            {
                active=line.substr(string("active_branch:").size());
                break;
            }
        }
        return active.empty()?"main":active;
    }
    void setActiveBranch(const string &branch) 
    {
        ofstream configOut(configFile);
        if(!configOut) 
        {
            cerr<<"Error: Could not open config file for writing.\n";
            return;
        }
        configOut<<"active_branch:"<<branch<<"\n";
    }
    void init() 
    {
        if(fs::exists(vcsDir))
        {
            cout<<"Repository already exists\n";
            return;
        }
        fs::create_directories(vcsDir);
        fs::create_directories(stagingDir);
        fs::create_directories(commitsDir);
        ofstream configOut(configFile);
        configOut<<"active_branch:main\n";
        fs::create_directories(commitsDir/"main");
        ofstream logFile(commitsDir/"main"/"log.txt");
        cout<<"Initialized repository vcs with active branch 'main'.\n";
    }
    void add(const string &filename) 
    {
        if(!fs::exists(filename)) 
        {
            cerr<<"File not found: "<<filename<<"\n";
            return;
        }
        ifstream fileIn(filename);
        if(!fileIn) 
        {
            cerr<<"Failed to open file: " << filename << "\n";
            return;
        }
        ostringstream buffer;
        buffer<<fileIn.rdbuf();
        string fileContent=buffer.str();
        string sanitizedFilename=fs::path(filename).filename().string();
        string branch=getActiveBranch();
        fs::path branchStagingDir=stagingDir/branch;
        fs::create_directories(branchStagingDir);
        ofstream stagedFile(branchStagingDir/(sanitizedFilename+".stage")); 
        stagedFile<<fileContent;
        cout<<"Added content of "<<filename<<" to staging area.\n";
    }
    map<string,string> getStagedFiles() 
    {
        map<string,string>staged;
        string branch=getActiveBranch();
        fs::path branchStagingDir=stagingDir/branch;
        if(!fs::exists(branchStagingDir))
        return staged;
        for(const auto &entry:fs::directory_iterator(branchStagingDir)) 
        {
            if(entry.path().extension()==".stage") 
            {
                ifstream fileIn(entry.path());
                ostringstream buffer;
                buffer<<fileIn.rdbuf();
                string content=buffer.str();
                string filename=entry.path().stem().string();
                staged[filename]=content;
            }
        }
        return staged;
    }
    void clearStaging() 
    {
        string branch=getActiveBranch();
        fs::path branchStagingDir=stagingDir/branch;
        if(fs::exists(branchStagingDir))
        for(const auto &entry:fs::directory_iterator(branchStagingDir)) 
        {
            if(entry.path().extension() == ".stage") 
            fs::remove(entry.path());
        }
    }
    void commit(const string &message) 
    {
        string branch=getActiveBranch();
        map<string,string>stagedFiles=getStagedFiles();
        if(stagedFiles.empty())
        {
            cerr<<"No files staged for commit.\n";
            return;
        }
        fs::path branchDir=commitsDir/branch;
        ofstream branchLog(branchDir/"log.txt",ios::app);
        string timestamp=getCurrentTimestamp();
        for(auto &[filename,currContent]:stagedFiles) 
        {
            fs::path fileCommitFolder=branchDir/filename;
            fs::create_directories(fileCommitFolder);
            fs::path graphPath=fileCommitFolder/"graph.txt";
            fs::path currentCommitPath=fileCommitFolder/"current_commit.txt";
            bool isFirstCommit=!fs::exists(graphPath);
            string parentCommit="NULL";
            string prevContent="";
            if(!isFirstCommit) 
            {
                ifstream currFile(currentCommitPath);
                if(currFile)
                getline(currFile,parentCommit);
                prevContent=reconstructFile(fileCommitFolder,parentCommit);
            }
            
            string diffData;
            string diffFileName;
            if(isFirstCommit) 
            {
                diffFileName="root.txt";
                ofstream rootFile(fileCommitFolder/diffFileName);
                rootFile<<currContent;
                diffData="";
            } 
            else 
            {
                diffFileName=generateHash(prevContent+currContent+message+timestamp);
                diffData=computeDiff(prevContent,currContent);
                ofstream diffOut(fileCommitFolder/(diffFileName+".diff"));
                diffOut<<diffData;
            }
            string commitID=isFirstCommit?generateHash(currContent+message+timestamp):diffFileName;
            GraphNode node;
            node.commitID=commitID;
            node.parentID=isFirstCommit?"NULL":parentCommit;
            node.message=message;
            node.timestamp=timestamp;
            node.diffFile=isFirstCommit?"root.txt":(diffFileName+".diff");
            updateGraph(graphPath, node);
            ofstream currCommitOut(currentCommitPath,ios::trunc);
            currCommitOut<<commitID;
            branchLog<<"Commit: "<<commitID<<", File: "<<filename<<", Message: "<<message<<", Time: "<<timestamp<<"\n";
            cout<<"Committed "<<filename<<" with commit id: "<<commitID<<"\n";
        }
        clearStaging();
    }
    void log() 
    {
        string branch=getActiveBranch();
        fs::path branchDir=commitsDir/branch;
        fs::path logPath=branchDir/"log.txt";
        ifstream logFile(logPath);
        if(!logFile) 
        {
            cerr<<"No commits in branch "<<branch<<"\n";
            return;
        }
        string line;
        while(getline(logFile, line))
        cout<<line<<"\n";
    }
    void checkout(const string &filename,const string &commitID) 
    {
        string branch=getActiveBranch();
        fs::path fileCommitFolder=commitsDir/branch/filename;
        if(!fs::exists(fileCommitFolder)) 
        {
            cerr<<"No commits found for file: "<<filename<<"\n";
            return;
        }
        string newContent=reconstructFile(fileCommitFolder,commitID);
        if(newContent.empty())
        {
            cerr<<"Could not reconstruct file "<<filename<<" at commit "<<commitID<<"\n";
            return;
        }
        ofstream outFile(filename);
        if(!outFile)
        {
            cerr<<"Error writing file: "<<filename<<"\n";
            return;
        }
        outFile<<newContent;
        ofstream currentCommit(fileCommitFolder/"current_commit.txt", ios::trunc);
        currentCommit<<commitID;
        cout<<"Checked out "<<filename<<" at commit "<<commitID<<"\n";
    }
    void currentCommit(const string &filename) 
    {
        string branch=getActiveBranch();
        fs::path fileCommitFolder=commitsDir/branch/filename;
        fs::path currentCommitPath=fileCommitFolder/"current_commit.txt";
        if(!fs::exists(currentCommitPath)) 
        {
            cerr<<"No commit information found for file: "<<filename<<"\n";
            return;
        }
        ifstream currCommitFile(currentCommitPath);
        string commitID;
        getline(currCommitFile,commitID);
        cout<<"Current commit for "<<filename<<": "<<commitID<<"\n";
    }
    void createBranch(const string &branchName) 
    {
        fs::path newBranchDir=commitsDir/branchName;
        if(fs::exists(newBranchDir)) 
        {
            cerr<<"Branch "<<branchName<<" already exists.\n";
            return;
        }
        fs::create_directories(newBranchDir);
        string activeBranch=getActiveBranch();
        fs::path activeBranchDir=commitsDir/activeBranch;
        if(fs::exists(activeBranchDir))
        for(const auto &entry:fs::directory_iterator(activeBranchDir))
        if(fs::is_directory(entry))
         {
            fs::path srcFolder=entry.path();
            fs::path destFolder=newBranchDir/srcFolder.filename();
            fs::create_directories(destFolder);
            fs::path srcGraph = srcFolder/"graph.txt";
            fs::path destGraph = destFolder/"graph.txt";
            if(fs::exists(srcGraph))
            fs::copy(srcGraph,destGraph,fs::copy_options::overwrite_existing);
            fs::path srcCurrent=srcFolder/"current_commit.txt";
            fs::path destCurrent=destFolder/"current_commit.txt";
            if(fs::exists(srcCurrent))
            fs::copy(srcCurrent,destCurrent,fs::copy_options::overwrite_existing);
            ifstream currFile(srcCurrent);
            string commitID;
            getline(currFile,commitID);
            currFile.close();
            string content=reconstructFile(srcFolder,commitID);
            ofstream rootOut(destFolder/"root.txt",ios::trunc);
            if(rootOut)
            rootOut<<content;
            else
            cerr<<"Error writing new root.txt for "<<srcFolder.filename().string()<<"\n";
        }
        ofstream(newBranchDir/"log.txt");
        cout<<"Created branch "<<branchName<<"\n";
    }
    void switchBranch(const string &branchName) 
    {
        fs::path branchDir=commitsDir/branchName;
        if(!fs::exists(branchDir)) 
        {
            cerr<<"Branch "<<branchName<<" does not exist.\n";
            return;
        }
        setActiveBranch(branchName);
        for(const auto &entry:fs::directory_iterator(branchDir))
        if(fs::is_directory(entry))
        {
            fs::path fileFolder=entry.path();
            string filename=fileFolder.filename().string();
            fs::path currentCommitPath=fileFolder / "current_commit.txt";
            if(fs::exists(currentCommitPath))
            {
                ifstream currCommitFile(currentCommitPath);
                string commitID;
                getline(currCommitFile,commitID);
                currCommitFile.close();
                fs::path rootPath=fileFolder/"root.txt";
                ifstream rootFile(rootPath);
                if(rootFile)
                {
                    ostringstream oss;
                    oss<<rootFile.rdbuf();
                    string content=oss.str();
                    ofstream outFile(filename,ios::trunc);
                    if(outFile)
                    {
                        outFile<<content;
                        cout<<"Checked out "<<filename<<" at commit "<<commitID << "\n";
                    }
                    else 
                    cerr<<"Error writing file: "<<filename<<"\n";
                }
                else 
                cerr<<"Root file missing for "<<filename << "\n";
            }
        }
        cout << "Switched to branch " << branchName << "\n";
    }
    void currentBranch() 
    {
        cout<<"Active branch: "<<getActiveBranch()<<"\n";
    }
    void listBranches()
    {
        cout<<"Branches:\n";
        for(const auto&entry:fs::directory_iterator(commitsDir))
        if(fs::is_directory(entry))
        cout<<"-"<<entry.path().filename().string()<<"\n";
    }
    void listFiles() 
    {
        string branch=getActiveBranch();
        fs::path branchDir=commitsDir/branch;
        if(!fs::exists(branchDir)) 
        {
            cerr<<"Branch directory "<<branchDir<<" does not exist.\n";
            return;
        }
        cout<<"Files tracked in branch '"<<branch<<"':\n";
        for(const auto &entry:fs::directory_iterator(branchDir)) 
        {
            if(fs::is_directory(entry.path()))
            cout<<"- "<<entry.path().filename().string()<<"\n";
        }
    }
    void status() 
    {
        cout<<"Active branch: "<<getActiveBranch()<<"\n";
        map<string, string> staged=getStagedFiles();
        if(staged.empty())
        cout<<"No files staged.\n";
        else 
        {
            cout<<"Staged files:\n";
            for(const auto &entry:staged)
            cout<<"  "<<entry.first<<"\n";
        }
    }
    string simpleHunkDiff(const string &oldContent,const string &newContent) 
    {
        vector<string>oldLines=splitLines(oldContent);
        vector<string>newLines=splitLines(newContent);
        int m=oldLines.size(),n =newLines.size();
        vector<vector<int>>lcs(m+1,vector<int>(n+1,0));
        for(int i=1;i<=m;i++)
        for(int j=1;j<=n;j++)
        if(oldLines[i-1]==newLines[j-1])
        lcs[i][j]=lcs[i-1][j-1]+1;
        else
        lcs[i][j]=max(lcs[i-1][j],lcs[i][j-1]);
        int i=m,j=n;
        struct DiffOp 
        {
            int oldPos,newPos; 
            string op;     
            string text;
        };
        vector<DiffOp>ops;
        while(i>0&&j>0) 
        {
            if(oldLines[i-1]==newLines[j-1])
            {
                i--; 
                j--;
            }
            else if(lcs[i- 1][j]>=lcs[i][j-1]) 
            {
                ops.push_back({i-1,j,"-",oldLines[i-1]});
                i--;
            } 
            else 
            {
                ops.push_back({i,j-1,"+",newLines[j-1]});
                j--;
            }
        }
        while(i>0) 
        {
            ops.push_back({i-1,j,"-",oldLines[i-1]});
            i--;
        }
        while(j>0) 
        {
            ops.push_back({i,j-1,"+",newLines[j - 1]});
            j--;
        }
        reverse(ops.begin(),ops.end());
        stringstream output;
        int start = 0;
        while(start<ops.size())
        {
            int hunkOldStart=ops[start].oldPos; 
            int hunkNewStart=ops[start].newPos; 
            int end=start+1;
            while(end<ops.size()&&((ops[end].oldPos==ops[end-1].oldPos+1)||(ops[end].newPos==ops[end-1].newPos+1)))
            end++;
            int delCount=0,addCount=0;
            for(int k=start;k<end;k++)
            if(ops[k].op=="-")
            delCount++;
            else if(ops[k].op=="+")
            addCount++;
            output<<"@@ -"<<(hunkOldStart+1)<<","<<delCount<<" +"<<(hunkNewStart+1)<<","<<addCount<<" @@\n";
            for(int k=start;k<end;k++)
            output<<ops[k].op<<" "<<ops[k].text<<"\n";
            start = end;
        }
        return output.str();
    }    
    void diffFile(const string &filename) 
    {
        string branch=getActiveBranch();
        fs::path fileCommitFolder=commitsDir/branch/filename;
        fs::path currentCommitPath=fileCommitFolder/"current_commit.txt";
        if(!fs::exists(currentCommitPath))
        {
            cerr<<"No committed version found for file: "<<filename<<"\n";
            return;
        }
        ifstream commitIn(currentCommitPath);
        string currentCommitID;
        getline(commitIn,currentCommitID);
        string committedContent=reconstructFile(fileCommitFolder,currentCommitID);
        if(committedContent.empty()) 
        {
            cerr<<"Failed to reconstruct committed version.\n";
            return;
        }
        ifstream workingFile(filename);
        if(!workingFile) 
        {
            cerr<<"Cannot open file: "<<filename<<"\n";
            return;
        }
        ostringstream oss;
        oss<<workingFile.rdbuf();
        string workingContent=oss.str();
        string diffOutput=simpleHunkDiff(committedContent,workingContent);
        cout<<diffOutput;
    }
    void cloneRepo(const std::string& remote, const std::string& targetDir) {
        const std::string zipFile = "vcs.zip";
        const std::string errorFile = "clone_error.txt";
    
        std::string curlCmd = "curl -f -o " + zipFile + " http://" + remote + "/clone 2> " + errorFile;
        if (std::system(curlCmd.c_str()) != 0) {
            std::ifstream err(errorFile);
            std::string msg((std::istreambuf_iterator<char>(err)), {});
            std::cerr << "Clone error: " << msg << "\n";
            ::remove(errorFile.c_str());
            return;
        }
    
        std::string unzipCmd = "unzip -o " + zipFile + " -d " + targetDir + " 2> " + errorFile;
        if (std::system(unzipCmd.c_str()) != 0) {
            std::ifstream err(errorFile);
            std::string msg((std::istreambuf_iterator<char>(err)), {});
            std::cerr << "Unzip error: " << msg << "\n";
            ::remove(zipFile.c_str());
            ::remove(errorFile.c_str());
            return;
        }
    
        fs::path metaPath = fs::path(targetDir) / vcsDir / "vcs_meta.txt";
        fs::create_directories(metaPath.parent_path());
        std::ofstream metaOut(metaPath);
        if (metaOut) {
            metaOut << "remote_url:" << remote << "\n";
            std::cout << "Cloned repository to " << targetDir << "\n";
        }
    
        ::remove(zipFile.c_str());
        ::remove(errorFile.c_str());
    }
    
    void push(const std::string &remote) {
        const std::string zipFile = "vcs_push.zip";
        std::string remoteUrl = remote.empty() ? getRemote() : remote;
        
        if (remoteUrl.empty()) {
            std::cerr << "Error: No remote URL configured.\n";
            std::cerr << "You need to either:\n";
            std::cerr << "1. Clone first: ./vcs clone <remote-url> <dir>\n";
            std::cerr << "2. Or specify remote: ./vcs push <remote-url>\n";
            std::cerr << "3. Or set default remote: ./vcs set-remote <remote-url>\n";
            return;
        }
    
        std::string zipCmd = "zip -rq " + zipFile + " " + commitsDir.string();
        if (std::system(zipCmd.c_str()) != 0) {
            std::cerr << "Failed to zip commits.\n";
            return;
        }
    
        std::string cmd = "curl -s --fail -X POST -F \"file=@" + zipFile + "\" http://" + remoteUrl + "/push";
        int result = std::system(cmd.c_str());
        ::remove(zipFile.c_str());
    
        if (result == 0) std::cout << "Push complete.\n";
        else std::cerr << "Push failed.\n";
    }
    
    void pull(const std::string &remote) {
        const std::string zipFile = "vcs.zip";
        std::string remoteUrl = remote.empty() ? getRemote() : remote;
        if (remoteUrl.empty()) {
            std::cerr << "No remote URL configured.\n";
            return;
        }
    
        std::string curlCmd = "curl -s --fail http://" + remoteUrl + "/pull --output " + zipFile;
        if (std::system(curlCmd.c_str()) != 0) {
            std::cerr << "Pull failed.\n";
            return;
        }
    
        if (std::system(("unzip -o " + zipFile).c_str()) != 0)
            std::cerr << "Unzip failed.\n";
        else
            std::cout << "Pulled from " << remoteUrl << "\n";
    
        ::remove(zipFile.c_str());
    }
    
    
    void startServer() {
        if (!fs::exists(vcsDir)) init();
    
        if (isPortInUse(9000)) {
            std::cerr << "Port 9000 is in use.\n";
            return;
        }
    
        int res = std::system("python3 server.py & echo $! > .vcs/server.pid");
        if (res != 0) {
            std::cerr << "Failed to start server.\n";
            return;
        }
    
        std::this_thread::sleep_for(std::chrono::seconds(1));
    
        std::ifstream infoFile(vcsDir / "server.info");
        std::ifstream pidFile(vcsDir / "server.pid");
    
        std::string serverAddr, pid;
        getline(infoFile, serverAddr);
        getline(pidFile, pid);
    
        std::ofstream metaOut(metadataFile);
        if (metaOut) {
            metaOut << "serverpid:" << pid << "\n";
            metaOut << "remote_url:" << serverAddr << "\n";
        }
    
        std::cout << "Server running at " << serverAddr << "\n";
    }
    
    void stopServer() {
        if (!fs::exists(metadataFile)) {
            std::cerr << "No metadata found.\n";
            return;
        }
    
        std::ifstream metaIn(metadataFile);
        std::string line, pid;
        while (getline(metaIn, line)) {
            if (line.find("serverpid:") == 0) {
                pid = line.substr(10);
                break;
            }
        }
    
        if (pid.empty()) {
            std::cerr << "PID not found.\n";
            return;
        }
    
        std::string cmd = "kill " + pid;
        if (std::system(cmd.c_str()) == 0) {
            std::cout << "Server stopped.\n";
            fs::remove(vcsDir / "server.pid");
            fs::remove(vcsDir / "server.info");
        } else {
            std::cerr << "Failed to stop server.\n";
        }
    }
    

bool isPortInUse(int port) {
    std::string cmd = "lsof -i :" + std::to_string(port) + " > /dev/null";
    return std::system(cmd.c_str()) == 0;
}
// Save remote URL to config.txt
void setRemote(const std::string& remoteUrl) {
    fs::create_directories(vcsDir);
    std::ofstream configOut(metadataFile, std::ios::app);
    if (!configOut) {
        std::cerr << "Error: Unable to write to config file.\n";
        return;
    }
    configOut << "remote_url:" << remoteUrl << "\n";
    std::cout << "Remote URL set to: " << remoteUrl << "\n";
}

// Load saved remote URL from config.txt
std::string getRemote() {
    if (!fs::exists(configFile)) return "";
    std::ifstream config(metadataFile);
    std::string line;
    while (getline(config, line)) {
        if (line.find("remote_url:") == 0) {
            return line.substr(std::string("remote_url:").length());
        }
    }
    return "";
}


void printUsage() {
    cout<<"Usage: ./vcs <command> [arguments]\n";
    cout<<"Commands:\n";
    cout<<"  init                       Initialize repository\n";
    cout<<"  add <filename>             Stage a file\n";
    cout<<"  commit <message>           Commit staged files\n";
    cout<<"  log                        Show commit history\n";
    cout<<"  checkout <file> <commit>   Checkout file at commit\n";
    cout<<"  status                     Show repo status\n";
    cout<<"  createbranch <branch>      Create new branch\n";
    cout<<"  switch <branch>            Switch branch\n";
    cout<<"  current-branch             Show active branch\n";
    cout<<"  branches                   List branches\n";
    cout<<"  files                      List tracked files\n";
    cout<<"  diff <filename>            Show changes\n";
    cout<<"  clone <remote> <local>     Clone a repository\n";
    cout<<"  push [remote]              Push changes (remote optional)\n";
    cout<<"  pull [remote]              Pull changes (remote optional)\n";
    cout<<"  start                      Start repository server\n";
    cout<<"  stop                       Stop server\n";
}
};
int main(int argc, char* argv[]) {
    if(argc < 2) {
        VersionControl().printUsage();
        return 1;
    }
    VersionControl vcs;
    string command = argv[1];
    
    if(command == "init") vcs.init();
    else if(command == "add" && argc > 2) vcs.add(argv[2]);
    else if(command == "commit" && argc > 2) vcs.commit(argv[2]);
    else if(command == "log") vcs.log();
    else if(command == "checkout" && argc > 3) vcs.checkout(argv[2], argv[3]);
    else if(command == "status") vcs.status();
    else if(command == "createbranch" && argc > 2) vcs.createBranch(argv[2]);
    else if(command == "switch" && argc > 2) vcs.switchBranch(argv[2]);
    else if(command == "current-branch") vcs.currentBranch();
    else if(command == "branches") vcs.listBranches();
    else if(command == "files") vcs.listFiles();
    else if(command == "diff" && argc > 2) vcs.diffFile(argv[2]);
    else if(command == "clone" && argc > 3) vcs.cloneRepo(argv[2], argv[3]);
    else if(command == "push") vcs.push(argc > 2 ? argv[2] : "");
    else if(command == "pull") vcs.pull(argc > 2 ? argv[2] : "");
    else if(command == "start") vcs.startServer();
    else if(command == "stop") vcs.stopServer();
    else {
        vcs.printUsage();
        return 1;
    }
    return 0;
}