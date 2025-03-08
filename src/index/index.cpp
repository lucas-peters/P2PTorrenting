# include "index.hpp"

namespace torrent_p2p {
    Index::Index(int port) : Node(port) {
        start();      // TODO
    }


void Index::start() {
    running_ = true;
    // keeps io_context.run() from exiting immediately;
}

std::vector<std::string> Index::generateKeywords(std::string& title) {
    std::vector<std::string> keywords;
    std::string processedTitle = title;
    std::string currentWord;
    
    // Common words to filter out (stop words)
    static const std::unordered_set<std::string> stopWords = {
        "the", "it", "but", "on", "and", "for", "with", "this", "that", "from", "are", "was", "not",
        "but", "what", "all", "were", "when", "your", "can", "said", "there", "use",
        "each", "which", "she", "he", "his", "her", "how", "their", "will", "other", 
        "about", "out", "many", 
    };

    std::replace(processedTitle.begin(), processedTitle.end(), '_', ' ');
    std::replace(processedTitle.begin(), processedTitle.end(), '.', ' ');

    std::string spacedTitle;
    for (size_t i = 0; i < processedTitle.size(); i++) {
        char c = processedTitle[i];

        if (i < 1) {
            if(std::isupper(c)) {
                c = std::tolower(c);
            }
            spacedTitle += c;
            continue;
        }
        // remove camelcase and make all letters lower case
        if (std::isupper(c)) {
            if(std::islower(processedTitle[i - 1])) {
                spacedTitle += ' ';
            }
            c = std::tolower(c);
        }

        spacedTitle += c;
    }

    std::istringstream iss(spacedTitle);
    
    while (iss >> currentWord) {
        // remove empty words, short words, and words in our list
        if (currentWord.empty() || currentWord.length() < 3 || stopWords.find(currentWord) != stopWords.end()) {
            continue;
        }

        keywords.push_back(currentWord);
    }

    return keywords;
}

void Index::addTorrent(const std::string& title, const std::string& magnet) {
    // If we are already tracking this torrent don't bother
    if (titleToMagnet.find(title) != titleToMagnet.end())
        return;
    
    titleToMagnet[title] = magnet;

    std::vector<std::string> keywords;

    keywords = generateKeywords(title);
    for (auto& keyword : keywords) {
        if (keywordToTitle.find(keyword) == keywordToTitle.end()) {
            keywordToTitle[keyword] = std::vector<std::string> {title};
            continue;
        }
        auto& titles = keywordToTitle[keyword];
        if (std::find(titles.begin(), titles.end(), title) == titles.end()) {
            titles.push_back(title);
        }
    }
}

std::vector<std::pair<std::string, std::string>> Index::searchTorrent(const std::string& keyword) {
    std::vector<std::pair<std::string, std::string>> pairs;
    if (keywordToTitle.find(keyword) == keywordToTitle.end())
        return pairs;

    std::vector<std::string> titles;

    titles = keywordToTitle[keyword];
    for (auto & title : titles) {
        pairs.push_back({title, titleToMagnet[title]});
    }
    return pairs;
}
} // namespace