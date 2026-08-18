#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

using namespace std;

const string RECORD_FILE = "last-file-record.txt";

// 判断隐藏文件/目录
bool isHidden(const string& path)
{
    DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES)
        return false;
    return (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
}

// 跳过无用系统缓存文件夹
bool shouldSkipFolder(const string& name)
{
    static const set<string> skipFolders = {
        "System Volume Information",
        "$Recycle.Bin",
        ".git",
        ".svn",
        "__pycache__",
        "node_modules",
        ".idea",
        ".vscode"
    };
    return skipFolders.count(name) > 0;
}

// 绝对路径 -> 相对于运行根目录的相对路径
string getRelativePath(const string& fullAbs, const string& rootAbs)
{
    if (fullAbs.substr(0, rootAbs.size()) == rootAbs)
    {
        string rel = fullAbs.substr(rootAbs.size());
        if (!rel.empty() && rel.front() == '\\')
            rel.erase(rel.begin());
        return rel;
    }
    return fullAbs;
}

// 递归遍历，收集运行目录下所有文件绝对路径
void getAllFiles(const string& root, vector<string>& absList)
{
    string search = root + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(search.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        string name = findData.cFileName;
        if (name == "." || name == "..")
            continue;

        string fullPath = root + "\\" + name;
        if (isHidden(fullPath))
            continue;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!shouldSkipFolder(name))
                getAllFiles(fullPath, absList);
        }
        else
        {
            absList.push_back(fullPath);
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

// 读取保存的相对路径记录
set<string> loadRecord()
{
    set<string> rec;
    ifstream fin(RECORD_FILE);
    string line;
    while (getline(fin, line))
    {
        if (!line.empty())
            rec.insert(line);
    }
    return rec;
}

// 只保存相对路径到记录文件
void saveRecord(const vector<string>& absFiles, const string& runRoot)
{
    ofstream fout(RECORD_FILE, ios::trunc);
    for (const auto& abs : absFiles)
    {
        fout << getRelativePath(abs, runRoot) << "\n";
    }
}

int main()
{
    system("chcp 65001 > nul");
    cout << "============================================\n";
    cout << "        Run Directory File Checker\n";
    cout << "============================================\n\n";

    // 获取程序运行工作目录
    char buf[MAX_PATH] = {0};
    GetCurrentDirectoryA(MAX_PATH, buf);
    string runRoot(buf);
    cout << "Scan Run Directory: " << runRoot << "\n\n";

    vector<string> currentAbs;
    getAllFiles(runRoot, currentAbs);

    // 转换为相对路径集合用于对比
    set<string> currentRelSet;
    for (const auto& abs : currentAbs)
    {
        currentRelSet.insert(getRelativePath(abs, runRoot));
    }

    set<string> oldRelSet = loadRecord();

    // 无记录文件，初始化存档
    if (oldRelSet.empty())
    {
        cout << "[Info] No history record found.\n";
        cout << "Create initial structure record? (Y/N default Y): ";
        char op;
        cin >> op;
        if (op == 'N' || op == 'n')
            return 0;

        saveRecord(currentAbs, runRoot);
        cout << "\n[Success] Initial structure saved.\n";
        system("pause");
        return 0;
    }

    vector<string> newItems, delItems;
    // 求差集对比相对路径
    set_difference(currentRelSet.begin(), currentRelSet.end(),
                   oldRelSet.begin(), oldRelSet.end(),
                   back_inserter(newItems));

    set_difference(oldRelSet.begin(), oldRelSet.end(),
                   currentRelSet.begin(), currentRelSet.end(),
                   back_inserter(delItems));

    if (!newItems.empty() || !delItems.empty())
    {
        cout << "[Detected] File structure changed!\n\n";

        if (!newItems.empty())
        {
            cout << "==== New Files ====\n";
            for (auto& s : newItems) cout << s << "\n";
            cout << "\n";
        }
        if (!delItems.empty())
        {
            cout << "==== Deleted Files ====\n";
            for (auto& s : delItems) cout << s << "\n";
            cout << "\n";
        }

        while (true)
        {
            cout << "Update record now? (Y/N): ";
            char op;
            cin >> op;
            if (op == 'Y' || op == 'y')
            {
                saveRecord(currentAbs, runRoot);
                cout << "\n[Success] Record updated.\n";
                break;
            }
            else if (op == 'N' || op == 'n')
            {
                cout << "\n[Skip] Record unchanged.\n";
                break;
            }
        }
    }
    else
    {
        cout << "[OK] No changes in file structure.\n";
    }

    system("pause");
    return 0;
}