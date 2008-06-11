/*
Copyright (C) 2006 Pedro Felzenszwalb

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
*/

#ifndef DISJOINT_SET
#define DISJOINT_SET

// disjoint-set forests using union-by-rank and path compression (sort of).

class disjoint_set_forest {
    public:
        class element {
            public:
                int rank;
                int p;
                int size;
        };

        disjoint_set_forest(int elements);
        ~disjoint_set_forest();
        int find(int x);  
        void join(int x, int y);
        int size(int x) const { return elts[x].size; }
        int num_sets() const { return num; }

    private:
        element *elts;
        int num;
};

disjoint_set_forest::disjoint_set_forest(int elements) {
    elts = new element[elements];
    num = elements;
    for (int i = 0; i < elements; i++) {
        elts[i].rank = 0;
        elts[i].size = 1;
        elts[i].p = i;
    }
}

disjoint_set_forest::~disjoint_set_forest() {
    delete [] elts;
}

int disjoint_set_forest::find(int x) {
    int y = x;
    while (y != elts[y].p)
        y = elts[y].p;
    elts[x].p = y;
    return y;
}

void disjoint_set_forest::join(int x, int y) {
    if (elts[x].rank > elts[y].rank) {
        elts[y].p = x;
        elts[x].size += elts[y].size;
    } else {
        elts[x].p = y;
        elts[y].size += elts[x].size;
        if (elts[x].rank == elts[y].rank)
            elts[y].rank++;
    }
    num--;
}

#endif
