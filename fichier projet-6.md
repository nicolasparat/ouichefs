# Architecture de la solution

La fonction read lit l'index block, puis itère sur les extents et lit leurs blocs tant qu'il reste à lire (len > 0). On s'assure au début que len n'aille pas plus loin que la fin du fichier.

La fonction write calcule la position de "fin de l'écriture" puis lit l'index block et itère sur les extents (et leur blocs) tant qu'il reste à écrire (et qu'il reste des blocks libres), elle écrit dans ces blocs. Si on atteint le stade ou il faut créer de nouveaux blocs (on rallonge le fichier), on commence par regarder s'il y en a dans la réserve. S'il n'y en a pas, on itère sur la bfreemap pour trouver le premier segment contigu de [taille de la réserve] blocs, on retourne le premier et on ajoute les autres à la réserve. Si on ne trouve aucun segment de cette longeur, on garbage collect avant de réessayer. Le bloc qu'on "retourne" est ajouté à la liste des extents (on tient compte du fait qu'il soit contigu ou non avec le dernier bloc du dernier extent). Si on doit créer un nouvel extent et qu'on se rend compte qu'il n'y en a aucun de disponible, on annule la procédure (et désalloue le bloc). Tous les cas "litigieux" sont gérés par le fait qu'on utilise un loop qui essaye d'écrire tant qu'il reste à écrire avec des breaks sous certaines conditions (erreur / plus de bloc / plus d'extent). Ainsi, si on cherche à écrire une longueur supérieure à la réserve, par exemple, on demandera les blocs un par un jusqu'à vider la réserve, puis la demande suivante (avec la réserve vide) se chargera de réallouer une réserve et de retourner un bloc : on essaye indéfiniment d'écrire jusqu'à ne plus rien avoir à écrire ou rencontrer une erreur.
Enfin, on update les métadonnées de l'inode et retourne la quantité de blocs écrits.

Lors du unlink, on fait attention à ne reset la réserve que si nlink <= 1 pour un fichier et <= 2 pour un dossier, sinon on risquerait de la reset alors qu'elle est encore utilisée. 

Le garbage collector itère sur les réserves et libère les blocs qu'elles contiennent mais n'utilise pour l'instant pas de lock, il est donc sujet aux race conditions. Je n'ai pas non plus réussi à le tester car mon test semble bugué (test_gc.c).

# Travail réalisé
Pour chaque section ci-dessous, donnez une des indications suivantes :
- Testé et fonctionnel
- Pas complètement fonctionnel (bug, etc..), dans ce cas, précisez les problèmes
- Non traité

## 1.2 Reimplementation of the read and the write functions
- Testé et fonctionnel

## 1.3 Extent-based index block (1.4 read ; 1.5 write)
- Testé et fonctionnel

## 1.6 Contiguous block allocator
- Testé et fonctionnel

## 1.7 Write-time block reservation
- Testé et fonctionnel

## 1.7.4 Garbage collector
- Implémenté mais impossible de dire s'il fonctionne : je n'ai pas réussi à faire fonctionner le test
- Je n'ai mis aucun lock pour l'instant, donc il y aurait des race conditions s'il fonctionnait

## 1.8 Sysfs statistics
- Non traité

## 1.9 Sparse files and holes (read)
- Non traité

## 1.9 Sparse files and holes (write in hole)
- Non traité

## 1.10 Bonus: File defragmentation
- Non traité

## 1.11 MEGA Bonus: advanced block allocator
- Non traité

# Remarques
Les tests sont dans Documentation.md