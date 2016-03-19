#include <string>
#include <iostream>
#include <assert.h>
#include <vector>
#include <sstream>
#include <random>
#include <map>
#include "Transaction.hh"
#include "TVector.hh"
#include "TVector_nopred.hh"
#include "clp.h"
#include "randgen.hh"
#include <sys/time.h>
int max_range = 10000;
int max_value = 1000;
int global_seed = 0;
int nthreads = 2;
int ntrans = 1000000;
int opspertrans = 1;
int prepopulate = 1000;
int max_key = 1000;
double search_percent = 0.6;
double pushback_percent = 0.2; // pop_percent will be the same to keep the size of array roughly the same

int find_aborts[32];
uint32_t initial_seeds[64];
int unsuccessful_finds = 0;
TransactionTid::type lock;

typedef TVector<int> pred_data_structure;
typedef TVector_nopred<int> nopred_data_structure;


template <typename T>
struct TesterPair {
  T* t;
  int me;
};

template<class InputIt, class T>
InputIt findIt(InputIt first, InputIt last, const T& value)
{
  for (; first != last; ++first) {
    if (*first == value) {
      return first;
    }
  }
  return last;
}

template <typename T>
int findK(T* q, int val) {
  typename T::const_iterator fit = q->cbegin();
  typename T::const_iterator eit = q->cend();
  typename T::const_iterator it = findIt(fit, eit, val);
  return it-fit;
}

template <typename T>
void run_find_push_pop_get(T* q, int me) {
  TThread::set_id(me);
  std::uniform_int_distribution<long> slotdist(0, max_range);
  Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

  int N = ntrans/nthreads;
  int OPS = opspertrans;
  bool find_op = false;
  unsigned naborts = 0;

  for (int i = 0; i < N; ++i) {
    // so that retries of this transaction do the same thing
    Rand snap_transgen = transgen;
    while (1) {
      Sto::start_transaction();
      try {
        for (int j = 0; j < OPS; ++j) {
          find_op = false;
          int op = slotdist(transgen) % 100;
          if (op < search_percent * 100) {
            int val = slotdist(transgen) % int(max_key * 0.5); // This is always a successful find
            find_op = true;
            findK(q, val);
          } else if (op < (search_percent + pushback_percent) *100){
            int sz = q->size();
            if (sz < 1.1 * max_key) {
              int val = slotdist(transgen) % max_value;
              q->push_back(val);
            } else {
              q->pop_back();
            }
          } else if (op < (search_percent + 2*pushback_percent) * 100) {
            int sz = q->size();
            if (sz > 0.9*max_key) {
              q->pop_back();
            } else {
              int val = slotdist(transgen) % max_value;
              q->push_back(val);
            }
          } else {
            int sz = q->size();
            int key = slotdist(transgen) % (sz - 50);
            
            q->transGet(key);
          }
          
        }
        
        if (Sto::try_commit()) {
          break;
        }
        
      } catch (Transaction::Abort e) {
      }
      if (find_op)
        ++naborts;
      transgen = snap_transgen;
    }
  }
  find_aborts[me] = naborts;
}

template <typename T>
void run_push_pop(T* q, int me) {
    TThread::set_id(me);
    std::uniform_int_distribution<unsigned> slotdist(0, 999);
    Rand transgen(initial_seeds[2*me], initial_seeds[2*me + 1]);

    int N = ntrans/nthreads;
    int OPS = opspertrans;
    unsigned nretries = 0;
    std::ostringstream ob;
    unsigned kinds[3] = {0, 0, 0};

    for (int i = 0; i < N; ++i) {
        Rand transgen_snap = transgen;
        unsigned what;
        while (1) {
            Sto::start_transaction();
            what = 0;
            try {
                for (int j = 0; j < OPS; ++j) {
                    if (q->size() < 1001) {
                        q->push_back(slotdist(transgen));
                        what = 1;
                    } else if (q->size() > 4000) {
                        q->pop_back();
                        what = 2;
                    } else if ((*q)[slotdist(transgen)] >= 500)
                        q->pop_back();
                    else
                        q->push_back(slotdist(transgen));
                }
                if (Sto::try_commit())
                    break;
            } catch (Transaction::Abort) {}
            transgen = transgen_snap;
            ++nretries;
        }
        ++kinds[what];
    }

    //printf("%d: %u %u %u\n", me, kinds[0], kinds[1], kinds[2]);
    find_aborts[me] = nretries;
}

static const char* test_name = "find-push-pop-get";

template <typename T>
void* runFunc(void* x) {
    TesterPair<T>* tp = (TesterPair<T>*) x;
    if (strcmp(test_name, "find-push-pop-get") == 0)
        run_find_push_pop_get(tp->t, tp->me);
    else if (strcmp(test_name, "push-pop") == 0)
        run_push_pop(tp->t, tp->me);
    else
        abort();
    return nullptr;
}

template <typename T>
void startAndWait(int n, T* queue) {
  pthread_t tids[nthreads];
  TesterPair<T> *testers = new TesterPair<T>[n];
  for (int i = 0; i < nthreads; ++i) {
    testers[i].t = queue;
    testers[i].me = i;
    pthread_create(&tids[i], NULL, runFunc<T>, &testers[i]);
  }
  pthread_t advancer;
  pthread_create(&advancer, NULL, Transaction::epoch_advancer, NULL);
  pthread_detach(advancer);
  
  for (int i = 0; i < nthreads; ++i) {
    pthread_join(tids[i], NULL);
  }
}

void print_time(struct timeval tv1, struct timeval tv2) {
  printf("%f\n", (tv2.tv_sec-tv1.tv_sec) + (tv2.tv_usec-tv1.tv_usec)/1000000.0);
}

enum {
  opt_nthreads = 1, opt_ntrans, opt_opspertrans, opt_searchpercent, opt_pushbackpercent, opt_prepopulate, opt_seed
};

static const Clp_Option options[] = {
  { "nthreads", 0, opt_nthreads, Clp_ValInt, Clp_Optional },
  { "ntrans", 0, opt_ntrans, Clp_ValInt, Clp_Optional },
  { "opspertrans", 0, opt_opspertrans, Clp_ValInt, Clp_Optional },
  { "searchpercent", 0, opt_searchpercent, Clp_ValDouble, Clp_Optional },
  { "pushbackpercent", 0, opt_pushbackpercent, Clp_ValDouble, Clp_Optional },
  { "prepopulate", 0, opt_prepopulate, Clp_ValInt, Clp_Optional },
  { "seed", 0, opt_seed, Clp_ValInt, Clp_Optional },
};

static void help() {
  printf("Usage: [OPTIONS] [find-push-pop-get|push-pop]\n\
         Options:\n\
         --nthreads=NTHREADS (default %d)\n\
         --ntrans=NTRANS, how many total transactions to run (they'll be split between threads) (default %d)\n\
         --opspertrans=OPSPERTRANS, how many operations to run per transaction (default %d)\n\
         --searchpercent=SEARCHPERCENT, probability with which to do searches (default %f)\n\
         --pushbackpercent=PUSHBACKPERCENT, probability with which to do push backs (default %f)\n\
         --prepopulate=PREPOPULATE, prepopulate table with given number of items (default %d)\n\
         --seed=SEED, global seed to run the experiment \n",
         nthreads, ntrans, opspertrans, search_percent, pushback_percent, prepopulate);
  exit(1);
}

template <typename T>
void init(T* q) {
  std::uniform_int_distribution<long> slotdist(0, max_value - 1);
  Rand transgen(random(), random());
  for (int i = 0; i < prepopulate; i++) {
    TRANSACTION {
        q->push_back(i);
    } RETRY(false);
  }
}

template <typename T>
void run_and_report(const char* name) {
  struct timeval tv1,tv2;

  for (unsigned i = 0; i < arraysize(find_aborts); ++i)
    find_aborts[i] = 0;

  unsuccessful_finds = 0;
  T q;
  q.nontrans_reserve(4096);
  init(&q);

  gettimeofday(&tv1, NULL);

  startAndWait(nthreads, &q);

  gettimeofday(&tv2, NULL);
  int total_aborts = 0;
  for (int i = 0; i < 16; i++) {
    total_aborts += find_aborts[i];
  }
  printf("Retries: %i, unsuccessful finds: %i\n", total_aborts, unsuccessful_finds);
  printf("%s: ", name);
  print_time(tv1, tv2);

#if STO_PROFILE_COUNTERS
  Transaction::print_stats();
  {
    txp_counters tc = Transaction::txp_counters_combined();
    printf("total_n: %llu, total_r: %llu, total_w: %llu, total_searched: %llu, total_aborts: %llu (%llu aborts at commit time)\n", tc.p(txp_total_n), tc.p(txp_total_r), tc.p(txp_total_w), tc.p(txp_total_searched), tc.p(txp_total_aborts), tc.p(txp_commit_time_aborts));
  }
  Transaction::clear_stats();
#endif
}

int main(int argc, char *argv[]) {
  lock = 0;
  
  Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);
  
  int opt;
  while ((opt = Clp_Next(clp)) != Clp_Done) {
    switch (opt) {
      case opt_nthreads:
        nthreads = clp->val.i;
        break;
      case opt_ntrans:
        ntrans = clp->val.i;
        break;
      case opt_opspertrans:
        opspertrans = clp->val.i;
        break;
      case opt_searchpercent:
        search_percent = clp->val.d;
        break;
      case opt_pushbackpercent:
        pushback_percent = clp->val.d;
        break;
      case opt_prepopulate:
        prepopulate = clp->val.i;
        break;
      case opt_seed:
        global_seed = clp->val.i;
        break;
    case Clp_NotOption:
        test_name = clp->vstr;
        break;
      default:
        help();
    }
  }
  Clp_DeleteParser(clp);

  if (global_seed)
    srandom(global_seed);
  else
    srandomdev();
  for (unsigned i = 0; i < arraysize(initial_seeds); ++i)
    initial_seeds[i] = random();

    printf("%s, %d threads, %d trans, %d opspertrans\n", test_name, nthreads, ntrans, opspertrans);
#if !NDEBUG
    printf("THIS IS NOT AN NDEBUG RUN, predicates assertions are $$expensive$$\n");
#endif
    run_and_report<pred_data_structure>("Predicates");
    run_and_report<nopred_data_structure>("No predicates");
	return 0;
}
