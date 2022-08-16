
#include <chrono>
#include <algorithm>
#include <vector>

#include "plot_manager.h"
#include "segments.h"
#include "utils.h"


namespace Segs {

    void get_md_block(char *md_tag, int md_idx, int md_l, MdBlock *res) {
        int nmatches = 0;
        int del_length = 0;
        bool is_mm = false;
        while (md_idx < md_l) {
            if (48 <= md_tag[md_idx] && md_tag[md_idx] <= 57) {  // c is numerical
                nmatches = nmatches * 10 + md_tag[md_idx] - 48;
                md_idx += 1;
            } else {
                if (md_tag[md_idx] == 94) {  // del_sign is 94 from ord('^')
                    md_idx += 1;
                    while (65 <= md_tag[md_idx] && md_tag[md_idx] <= 90) {
                        md_idx += 1;
                        del_length += 1;
                    }
                } else {  // save mismatch
                    is_mm = true;
                    md_idx += 1;
                }
                break;
            }
        }
        res->matches = nmatches;
        res->md_idx = md_idx;
        res->is_mm = is_mm;
        res->del_length = del_length;
    }


    void get_mismatched_bases(std::vector<MMbase> &result,
                              char *md_tag, uint32_t r_pos,
                              uint32_t ct_l, uint32_t *cigar_p) {

        uint32_t opp, l, c_idx, s_idx, c_s_idx;
        size_t md_l = strlen(md_tag);
        std::deque<QueueItem> ins_q;
        MdBlock md_block;
        get_md_block(md_tag, 0, md_l, &md_block);
        if (md_block.md_idx == md_l) {
            return;
        }

        c_idx = 0;  // the cigar index
        s_idx = 0;  // sequence index of mismatches
        c_s_idx = 0;  // the index of the current cigar (c_idx) on the input sequence

        opp = cigar_p[0] & BAM_CIGAR_MASK;
        if (opp == 4) {
            c_idx += 1;
            s_idx += cigar_p[0] >> BAM_CIGAR_SHIFT;
            c_s_idx = s_idx;
        } else if (opp == 5) {
            c_idx += 1;
        }

        while (true) {
            if (c_idx < ct_l) {  // consume cigar until deletion reached, collect positions of insertions
                while (c_idx < ct_l) {
                    opp = cigar_p[c_idx] & BAM_CIGAR_MASK;
                    l = cigar_p[c_idx] >> BAM_CIGAR_SHIFT;
                    if (opp == 0 || opp == 8) {  // match
                        c_s_idx += l;
                    } else if (opp == 1) {  // insertion
                        ins_q.push_back({c_s_idx, l});
                        c_s_idx += l;
                    } else {  // opp == 2 or opp == 4 or opp == 5
                        break;
                    }
                    ++c_idx;
                }
                c_idx += 1;
            }

            while (true) {   // consume md tag until deletion reached
                if (!md_block.is_mm) {  // deletion or end or md tag
                    if (md_block.md_idx == md_l) {
                        return;
                    }
                    s_idx = c_s_idx;
                    r_pos += md_block.matches + md_block.del_length;
                    while (!ins_q.empty() &&
                           s_idx + md_block.matches >= ins_q[0].c_s_idx) {  // catch up with insertions
                        ins_q.pop_front();
                    }
                    get_md_block(md_tag, md_block.md_idx, md_l, &md_block);
                    break;
                }

                // check if insertion occurs before mismatch
                while (!ins_q.empty() && s_idx + md_block.matches >= ins_q[0].c_s_idx) {
                    s_idx += ins_q[0].l;
                    ins_q.pop_front();
                }
                s_idx += md_block.matches;
                r_pos += md_block.matches;
                result.push_back({s_idx, r_pos});
                s_idx += 1;
                r_pos += 1;
                get_md_block(md_tag, md_block.md_idx, md_l, &md_block);
            }
        }
    }

    void align_init(Align *self) {
        uint8_t *v;
        char *value;

        self->pos = self->delegate->core.pos;
        self->reference_end = bam_endpos(self->delegate);  // reference_end - already checked for 0 length cigar and mapped
        self->cov_start = self->pos;
        self->cov_end = self->reference_end;

        uint32_t pos, l, cigar_l, op, k;
        uint32_t *cigar_p;
        bam1_t *src;
        src = self->delegate;
        cigar_l = src->core.n_cigar;

        pos = src->core.pos;
        cigar_p = bam_get_cigar(src);

        self->left_soft_clip = 0;
        self->left_hard_clip = 0;
        self->right_soft_clip = 0;

        for (k = 0; k < cigar_l; k++) {
            op = cigar_p[k] & BAM_CIGAR_MASK;
            l = cigar_p[k] >> BAM_CIGAR_SHIFT;
            if (op == 4) {
                self->cov_end += l;
                self->right_soft_clip = l;
                if (k == 0) {
                    self->cov_start -= l;
                    self->left_soft_clip = l;

                } else {
                    self->cov_end += l;
                    self->right_soft_clip = l;
                }
            } else if (op == 1) {
                self->any_ins.push_back({pos, l});
            } else if (k == 0 && op == 5) {
                self->left_hard_clip = l;
            }

            if (op == BAM_CMATCH || op == BAM_CEQUAL || op == BAM_CDIFF) {
                self->block_starts.push_back(pos);
                self->block_ends.push_back(pos + l);
                pos += l;
            } else if (op == BAM_CDEL || op == BAM_CREF_SKIP) {
                pos += l;
            }
        }

        if (src->core.flag & 16) {  // reverse strand
            self->cov_start -= 1;   // pad between alignments
        } else {
            self->cov_end += 1;
        }

        v = bam_aux_get(self->delegate, "MD");
        if (v == nullptr) {
            self->has_MD = false;
        } else {
            value = (char *) bam_aux2Z(v);
            self->MD = value;
            self->has_MD = true;
        }
        if (bam_aux_get(self->delegate, "SA") != nullptr) {
            self->has_SA = true;
        }

        self->y = -1;
        self->polygon_height = 0.8;

        int ptrn = NORMAL;
        uint32_t flag = src->core.flag;
        if (flag & 1 && ~flag & 14) {  // not; proper-pair, unmapped, mate-unmapped
            if (src->core.tid == src->core.mtid) {
                if (self->pos <= src->core.mpos) {
                    if (~flag & 16) {
                        if (flag & 32) {
                            if (~flag & 2) {
                                ptrn = DEL;
                            }
                        } else {
                            ptrn = INV_F;
                        }
                    } else {
                        if (flag & 32) {
                            ptrn = INV_R;
                        } else {
                            ptrn = DUP;
                        }
                    }
                } else {
                    if (flag & 16) {
                        if (~flag & 32) {
                            if (~flag & 2) {
                                ptrn = DEL;
                            }
                        } else {
                            ptrn = INV_F;
                        }
                    } else {
                        if (~flag & 32) {
                            ptrn = INV_R;
                        } else {
                            ptrn = DUP;
                        }
                    }
                }
            } else {
                ptrn = TRA;
            }
        }

        self->orient_pattern = ptrn;

        if (flag & 2048 || self->has_SA) {
            self->edge_type = 2;  // "SPLIT"
        } else if (flag & 8) {
            self->edge_type = 3;  // "MATE_UNMAPPED"
        } else {
            self->edge_type = 1;  // "NORMAL"
        }

        get_mismatched_bases(self->mismatches, self->MD, self->pos, cigar_l, cigar_p);
        self->initialized = true;
    }

    void init_parallel(std::vector<Align> &aligns, int n) {
        if (n == 1) {
            for (size_t i = 0; i < aligns.size(); ++i)
                align_init(&aligns[i]);

        } else {
            BS::thread_pool pool(n);
            pool.parallelize_loop(0, aligns.size(),
                                  [&aligns](const int a, const int b) {
                                      for (int i = a; i < b; ++i)
                                          align_init(&aligns[i]);
                                  })
                    .wait();
        }
    }


    void addToCovArray(std::vector<int> &arr, Align *align, int begin, int end, int l_arr) {
        size_t n_blocks = align->block_starts.size();
        for (size_t idx=0; idx < n_blocks; ++idx) {

            uint32_t block_s = align->block_starts[idx];
            if (block_s >= end) { break; }

            uint32_t block_e = align->block_ends[idx];
            if (block_e < begin) { continue; }

            uint32_t s = (block_s >= begin) ? block_s - begin : 0;
            uint32_t e = (block_e < end) ? block_e - begin : l_arr;

            arr[s] += 1;  // s is uint32_t so can be < 0
            arr[e] -= 1;

//            std::cout << align->block_starts[idx] << " " << begin << std::endl;
//
//            uint32_t s = align->block_starts[idx] - begin;
//            uint32_t e = align->block_ends[idx] - begin;
//            if ( s < l_arr || e < l_arr ) {
//                arr[s] += 1;  // s is uint32_t so can be < 0
//                arr[(e > l_arr) ? l_arr : e] -= 1;
//            }
        }
    }

    int findY(int bamIdx, ReadCollection &rc, int vScroll, int linkType, Themes::IniOptions &opts, Utils::Region *region, linked_t &linked, bool joinLeft) {

        if (rc.readQueue.empty()) {
            return 0;
        }
        Align *q_ptr = &rc.readQueue.front();
        const char *qname;
        int i, j;

        // first find reads that should be linked together using qname
//        auto start = std::chrono::high_resolution_clock::now();
        if (linkType > 0) {
            // find the start and end coverage locations of aligns with same name
            for (i=0; i < (int)rc.readQueue.size(); ++i) {
                qname = bam_get_qname(q_ptr->delegate);
                if (linkType == 1) {
                    uint16_t flag = q_ptr->delegate->core.flag;
                    if (~flag & 2 || q_ptr->has_SA) {
                        linked[bamIdx][qname].push_back(i);
                    }
                } else {
                    linked[bamIdx][qname].push_back(i);
                }
                ++q_ptr;
            }

            // set all aligns with same name to have same start and end coverage locations
            for (i=0; i < (int)linked.size(); ++i) {
                map_t map = linked[i];
                for (auto const& keyVal : map) {
                    const std::vector<int> &ind = keyVal.second;
                    int size = (int)ind.size();
                    if (size < 2) {
                        break;
                    }
                    uint32_t cs = q_ptr[ind.front()].cov_start;
                    uint32_t ce = q_ptr[ind.back()].cov_end;
                    for (j=0; j < size; ++j) {
                        if (j > 0) {
                            q_ptr[j].cov_start = cs;
                        }
                        if (j < size - 2) {
                            q_ptr[j].cov_start = ce;
                        }
                    }
                }
            }
        }

        ankerl::unordered_dense::map< const char*, int > linkedSeen;  // Mapping of qname to y value

        std::vector<uint32_t> &ls = rc.levelsStart;
        std::vector<uint32_t> &le = rc.levelsEnd;

        if (ls.empty()) {
            ls.resize(opts.ylim + vScroll, 1215752191);
            le.resize(opts.ylim + vScroll, 0);
        }

        int qLen = (int)rc.readQueue.size();  // assume no overflow
        int stopCondition, move, si;
        int k = -1;

        int memLen = (int)ls.size();

        if (!joinLeft) {
            si = 0;
            stopCondition = qLen;
            move = 1;
        } else {
            si = qLen - 1;
            stopCondition = -1;
            move = -1;
        }
//        std::cout << memLen << " " << qLen << std::endl;

        if (si == 0) {
            q_ptr = &rc.readQueue.front();
        } else {
            q_ptr = &rc.readQueue.back();
        }

        while (si != stopCondition) {
            si += move;
            if (linkType > 0) {
                qname = bam_get_qname(q_ptr->delegate);
                if (linkedSeen.contains(qname)) {
                    q_ptr->y = linkedSeen[qname];
                    q_ptr += move;
                    continue;
                }
            }

            if (!joinLeft) {
                for (i=0; i < memLen; ++i) {
                    if (q_ptr->cov_start > le[i]) {
                        if (i > k) {// input is sorted so this is always true: s.cov_start < ol_start[i]:
                            ls[i] = q_ptr->cov_start;
                            k = (int)i;
                        }
                        if (i >= vScroll) {
                            q_ptr->y = i - vScroll;
                        }
                        if (linkType > 0 && linked[bamIdx].contains(qname)) {
                            linkedSeen[qname] = q_ptr->y;
                        }
                        le[i] = q_ptr->cov_end;
                        break;
                    }
                }
                if (i == memLen && linkType > 0 && linked[bamIdx].contains(qname)) {
                    linkedSeen[qname] = q_ptr->y;  // y is out of range i.e. -1
                }
                q_ptr += move;

            } else {
                for (i=0; i < memLen; ++i) {
                    if (q_ptr->cov_end < ls[i]) {
                        if (q_ptr->cov_end > le[i]) {
                            le[i] = q_ptr->cov_end;
                        }
                        if (i >vScroll) {
                            q_ptr->y = i - vScroll;
                        }
                        if (linkType > 0 && linked[bamIdx].contains(qname)) {
                            linkedSeen[qname] = q_ptr->y;
                        }
                        ls[i] = q_ptr->cov_start;
                        break;
                    }
                }
                if (i == memLen && linkType > 0 && linked[bamIdx].contains(qname)) {
                    linkedSeen[qname] = q_ptr->y;  // y is out of range i.e. -1
                }
                q_ptr += move;
            }
        }

        int samMaxY;
        if (!opts.tlen_yscale) {
            samMaxY = memLen - vScroll;
        } else {
            int regionSize = region->end - region->start;
            samMaxY = memLen;
            q_ptr = &rc.readQueue.front();
            for (i=0; i < (int)rc.readQueue.size(); ++i) {
                int tlen = (int)std::abs(q_ptr->delegate->core.isize);
                if (tlen < regionSize) {
                    q_ptr->y = tlen;
                    if (tlen > samMaxY) {
                        samMaxY = tlen;
                    }
                } else {
                    q_ptr->y = regionSize;
                    samMaxY = regionSize;
                }
                ++q_ptr;
            }
        }

//        auto finish = std::chrono::high_resolution_clock::now();
//        auto m = std::chrono::duration_cast<std::chrono::milliseconds >(finish - start);
//        std::cout << "Elapsed Time link: " << m.count() << " m seconds" << std::endl;

        return samMaxY;
    }

    void dropOutOfScope(std::vector< Utils::Region > &regions, std::vector< Segs::ReadCollection >& rcs, size_t nBams) {

        int idx = 0;
        for (size_t i=0; i < nBams; ++i) {

            for (size_t j=0; j < regions.size(); ++j) {

                uint32_t s = regions[i].start;
                uint32_t e = regions[i].end;

                std::vector<Align> &q = rcs[idx].readQueue;
                Align *q_ptr_b = &q.back();
                Align *q_ptr_f = &q.front();

                std::vector<uint32_t> &ls = rcs[idx].levelsStart;
                std::vector<uint32_t> &le = rcs[idx].levelsEnd;

                while (q_ptr_b != q_ptr_f) {
                    if (Utils::isOverlapping(s, e, q_ptr_b->cov_start, q_ptr_b->cov_end)) {
                        break;
                    } else {
                        le[q_ptr_b->y] = q_ptr_b->cov_start;
                        q.pop_back();
                    }
                    --q_ptr_b;
                }

                q_ptr_b = &q.back();
                int start_idx = 0;
                while (q_ptr_f != q_ptr_b) {
                    if (Utils::isOverlapping(s, e, q_ptr_f->cov_start, q_ptr_f->cov_end)) {
                        break;
                    } else {
                        ls[q_ptr_f->y] = q_ptr_f->cov_end;
                        start_idx += 1;

                    }
                    ++q_ptr_f;
                }

                if (start_idx > 0) {
                    q.erase(q.begin(), q.begin() + start_idx);
                }

                idx += 1;

            }
        }
    };

}