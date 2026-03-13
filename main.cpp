#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_KEY_SIZE = 64;
const int BLOCK_SIZE = 4096;
const int NODE_DEGREE = 100;  // Adjusted for better performance

struct Key {
    char str[MAX_KEY_SIZE + 1];

    Key() { memset(str, 0, sizeof(str)); }
    Key(const char* s) {
        memset(str, 0, sizeof(str));
        strncpy(str, s, MAX_KEY_SIZE);
    }

    bool operator<(const Key& other) const {
        return strcmp(str, other.str) < 0;
    }
    bool operator==(const Key& other) const {
        return strcmp(str, other.str) == 0;
    }
    bool operator<=(const Key& other) const {
        return strcmp(str, other.str) <= 0;
    }
};

struct Pair {
    Key key;
    int value;

    Pair() : value(0) {}
    Pair(const Key& k, int v) : key(k), value(v) {}

    bool operator<(const Pair& other) const {
        if (key == other.key) return value < other.value;
        return key < other.key;
    }
    bool operator==(const Pair& other) const {
        return key == other.key && value == other.value;
    }
};

class BPlusTree {
private:
    struct Node {
        bool is_leaf;
        int size;
        int parent;
        int next;  // For leaf nodes
        Pair pairs[NODE_DEGREE];
        int children[NODE_DEGREE + 1];

        Node() : is_leaf(true), size(0), parent(-1), next(-1) {
            fill(children, children + NODE_DEGREE + 1, -1);
        }
    };

    fstream file;
    string filename;
    int root;
    int node_count;

    Node read_node(int pos) {
        Node node;
        file.seekg(sizeof(int) * 2 + pos * sizeof(Node));
        file.read((char*)&node, sizeof(Node));
        return node;
    }

    void write_node(int pos, const Node& node) {
        file.seekp(sizeof(int) * 2 + pos * sizeof(Node));
        file.write((const char*)&node, sizeof(Node));
        file.flush();
    }

    void write_header() {
        file.seekp(0);
        file.write((const char*)&root, sizeof(int));
        file.write((const char*)&node_count, sizeof(int));
        file.flush();
    }

    int create_node(bool is_leaf) {
        Node node;
        node.is_leaf = is_leaf;
        int pos = node_count++;
        write_node(pos, node);
        write_header();
        return pos;
    }

    void split_leaf(int node_idx, int parent_idx, int child_pos) {
        Node node = read_node(node_idx);
        int mid = node.size / 2;

        Node new_node;
        new_node.is_leaf = true;
        new_node.size = node.size - mid;
        new_node.next = node.next;

        for (int i = 0; i < new_node.size; i++) {
            new_node.pairs[i] = node.pairs[mid + i];
        }

        node.size = mid;
        node.next = node_count;

        int new_idx = create_node(true);
        new_node.parent = parent_idx;
        node.parent = parent_idx;

        write_node(node_idx, node);
        write_node(new_idx, new_node);

        insert_in_parent(node_idx, new_node.pairs[0], new_idx, parent_idx);
    }

    void split_internal(int node_idx, int parent_idx) {
        Node node = read_node(node_idx);
        int mid = node.size / 2;

        Node new_node;
        new_node.is_leaf = false;
        new_node.size = node.size - mid - 1;

        for (int i = 0; i < new_node.size; i++) {
            new_node.pairs[i] = node.pairs[mid + 1 + i];
            new_node.children[i] = node.children[mid + 1 + i];
        }
        new_node.children[new_node.size] = node.children[node.size];

        Pair up_pair = node.pairs[mid];
        node.size = mid;

        int new_idx = create_node(false);
        new_node.parent = parent_idx;
        node.parent = parent_idx;

        // Update children's parent
        for (int i = 0; i <= new_node.size; i++) {
            if (new_node.children[i] != -1) {
                Node child = read_node(new_node.children[i]);
                child.parent = new_idx;
                write_node(new_node.children[i], child);
            }
        }

        write_node(node_idx, node);
        write_node(new_idx, new_node);

        insert_in_parent(node_idx, up_pair, new_idx, parent_idx);
    }

    void insert_in_parent(int left_idx, const Pair& pair, int right_idx, int parent_idx) {
        if (parent_idx == -1) {
            // Create new root
            Node new_root;
            new_root.is_leaf = false;
            new_root.size = 1;
            new_root.pairs[0] = pair;
            new_root.children[0] = left_idx;
            new_root.children[1] = right_idx;

            root = create_node(false);
            write_node(root, new_root);

            Node left = read_node(left_idx);
            left.parent = root;
            write_node(left_idx, left);

            Node right = read_node(right_idx);
            right.parent = root;
            write_node(right_idx, right);

            write_header();
            return;
        }

        Node parent = read_node(parent_idx);

        // Find position to insert
        int pos = 0;
        while (pos < parent.size && parent.pairs[pos] < pair) {
            pos++;
        }

        // Shift elements
        for (int i = parent.size; i > pos; i--) {
            parent.pairs[i] = parent.pairs[i - 1];
            parent.children[i + 1] = parent.children[i];
        }

        parent.pairs[pos] = pair;
        parent.children[pos + 1] = right_idx;
        parent.size++;

        write_node(parent_idx, parent);

        if (parent.size >= NODE_DEGREE) {
            split_internal(parent_idx, parent.parent);
        }
    }

    void insert_in_leaf(int node_idx, const Pair& pair) {
        Node node = read_node(node_idx);

        // Check if already exists
        for (int i = 0; i < node.size; i++) {
            if (node.pairs[i] == pair) {
                return;  // Already exists
            }
        }

        // Find position
        int pos = 0;
        while (pos < node.size && node.pairs[pos] < pair) {
            pos++;
        }

        // Shift
        for (int i = node.size; i > pos; i--) {
            node.pairs[i] = node.pairs[i - 1];
        }

        node.pairs[pos] = pair;
        node.size++;

        write_node(node_idx, node);

        if (node.size >= NODE_DEGREE) {
            split_leaf(node_idx, node.parent, -1);
        }
    }

    int find_leaf(const Key& key) {
        int current = root;

        while (true) {
            Node node = read_node(current);

            if (node.is_leaf) {
                return current;
            }

            // Find the correct child to follow
            int i = 0;
            while (i < node.size && !(key < node.pairs[i].key)) {
                i++;
            }

            current = node.children[i];
        }
    }

public:
    BPlusTree(const string& fname) : filename(fname), root(-1), node_count(0) {
        ifstream test(filename);
        bool exists = test.good();
        test.close();

        if (exists) {
            file.open(filename, ios::in | ios::out | ios::binary);
            file.read((char*)&root, sizeof(int));
            file.read((char*)&node_count, sizeof(int));
        } else {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            root = create_node(true);
            write_header();
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }

    void insert(const Key& key, int value) {
        Pair pair(key, value);

        if (root == -1) {
            root = create_node(true);
        }

        int leaf_idx = find_leaf(key);
        insert_in_leaf(leaf_idx, pair);
    }

    void remove(const Key& key, int value) {
        if (root == -1) return;

        int leaf_idx = find_leaf(key);
        Node node = read_node(leaf_idx);

        Pair target(key, value);
        int pos = -1;

        for (int i = 0; i < node.size; i++) {
            if (node.pairs[i] == target) {
                pos = i;
                break;
            }
        }

        if (pos == -1) return;  // Not found

        // Shift left
        for (int i = pos; i < node.size - 1; i++) {
            node.pairs[i] = node.pairs[i + 1];
        }
        node.size--;

        write_node(leaf_idx, node);
    }

    vector<int> find(const Key& key) {
        vector<int> result;

        if (root == -1) return result;

        int leaf_idx = find_leaf(key);

        while (leaf_idx != -1) {
            Node node = read_node(leaf_idx);

            bool found_any = false;
            for (int i = 0; i < node.size; i++) {
                if (node.pairs[i].key == key) {
                    result.push_back(node.pairs[i].value);
                    found_any = true;
                } else if (found_any) {
                    // Keys are sorted, so we can stop
                    break;
                }
            }

            // Check next leaf
            if (node.next != -1 && node.size > 0) {
                Node next_node = read_node(node.next);
                if (next_node.size > 0 && next_node.pairs[0].key == key) {
                    leaf_idx = node.next;
                } else {
                    break;
                }
            } else {
                break;
            }
        }

        sort(result.begin(), result.end());
        return result;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(0);

    BPlusTree tree("data.db");

    int n;
    cin >> n;

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            char index[MAX_KEY_SIZE + 1];
            int value;
            cin >> index >> value;
            tree.insert(Key(index), value);
        } else if (cmd == "delete") {
            char index[MAX_KEY_SIZE + 1];
            int value;
            cin >> index >> value;
            tree.remove(Key(index), value);
        } else if (cmd == "find") {
            char index[MAX_KEY_SIZE + 1];
            cin >> index;
            vector<int> results = tree.find(Key(index));

            if (results.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < results.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << results[j];
                }
                cout << "\n";
            }
        }
    }

    return 0;
}
